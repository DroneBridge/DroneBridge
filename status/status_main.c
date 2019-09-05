/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   parts based on db_gnd_status by Rodizio. Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
 *   integrated into the DroneBridge extensions by Wolfgang Christl
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zconf.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <memory.h>
#include <sys/time.h>
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/shared_memory.h"
#include "../common/db_utils.h"
#include "../common/tcp_server.h"
#include "../common/db_raw_send_receive.h"
#include "../common/db_common.h"

#define NET_BUFF_SIZE 2048
#define MAX_TCP_CLIENTS 10

bool volatile keeprunning = true;
int num_inf_status = 0;
char db_mode;
uint8_t comm_id = DEFAULT_V2_COMMID;
char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];

void int_handler(int dummy) {
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]) {
    db_mode = DEFAULT_DB_MODE;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "n:m:c:?")) != -1) {
        switch (c) {
            case 'n':
                if (num_inf_status < DB_MAX_ADAPTERS) {
                    strncpy(adapters[num_inf_status], optarg, IFNAMSIZ);
                    num_inf_status++;
                }
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("This tool sends extra information about the video stream and RC via UDP to IP given by "
                       "IP-checker module. Use"
                       "\n\t-n Name of network interface"
                       "\n\t-m [w|m] default is <m>"
                       "\n\t-c <communication id> Choose a number from 0-255. Same on ground station and drone!");
                break;
            default:
                abort();
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    struct timespec timestamp;
    int restarts = 0, cardcounter = 0, select_return, max_sd, new_tcp_client, prev_seq_num_status = 0;
    struct timeval timecheck;
    long start, rightnow, status_message_update_rate = 100; // send status messages every 100ms (10Hz)
    int8_t best_dbm = 0;
    ssize_t l;
    uint8_t seq_num_status = 0;
    uint16_t radiotap_length;
    uint8_t lr_buffer[DATA_UNI_LENGTH];
    uint8_t message_buff[DATA_UNI_LENGTH - DB_RAW_V2_HEADER_LENGTH];
    uint8_t tcp_message_buff[NET_BUFF_SIZE];
    int tcp_clients[MAX_TCP_CLIENTS] = {0};

    db_rc_msg_t db_rc_status_message;
    db_rc_status_message.ident[0] = '$';
    db_rc_status_message.ident[1] = 'D';
    db_rc_status_message.message_id = 2;
    db_system_status_msg_t db_sys_status_message;
    db_sys_status_message.ident[0] = '$';
    db_sys_status_message.ident[1] = 'D';
    db_sys_status_message.message_id = 1;
    db_sys_status_message.mode = 'm';
    db_sys_status_message.recv_pack_sec = 0;
    db_sys_status_message.rssi_drone = 0;
    db_sys_status_message.voltage_status = 0;

    process_command_line_args(argc, argv);

    // open wbc rc shared memory to push rc rssi etc. to wbc OSD
    db_rc_status_t *db_rc_status_t = db_rc_status_memory_open();
    // open db rc shared memory
    db_rc_values_t *rc_values = db_rc_values_memory_open();
    // open db rc overwrite shared memory
    db_rc_overwrite_values_t *rc_overwrite_values = db_rc_overwrite_values_memory_open();
    // shm for video/gnd status
    db_gnd_status_t *db_gnd_status_t = db_gnd_status_memory_open();
    // all UAV status related data
    db_uav_status_t *db_uav_status = db_uav_status_memory_open();

    int number_cards = db_gnd_status_t->wifi_adapter_cnt;

    // set up long range receiving socket
    db_socket_t raw_interfaces_status[DB_MAX_ADAPTERS] = {0};
    for (int i = 0; i < num_inf_status; i++) {
        raw_interfaces_status[i] = open_db_socket(adapters[i], comm_id, db_mode, 6, DB_DIREC_DRONE, DB_PORT_STATUS,
                DB_FRAMETYPE_DEFAULT);
    }

    fd_set fd_socket_set;
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 100000; // 10Hz
    // Setup TCP server for GCS communication
    struct tcp_server_info_t status_tcp_server_info = create_tcp_server_socket(APP_PORT_STATUS);
    int tcp_addrlen = sizeof(status_tcp_server_info.servaddr);

    LOG_SYS_STD(LOG_INFO, "DB_STATUS_GND: started!\n");
    gettimeofday(&timecheck, NULL);
    start = (long) timecheck.tv_sec * 1000 + (long) timecheck.tv_usec / 1000;
    while (keeprunning) {
        socket_timeout.tv_sec = 0;
        socket_timeout.tv_usec = 100000; // 10Hz
        FD_ZERO(&fd_socket_set);
        FD_SET(status_tcp_server_info.sock_fd, &fd_socket_set);
        max_sd = status_tcp_server_info.sock_fd;
        for (int i = 0; i < num_inf_status; i++) {
            FD_SET(raw_interfaces_status[i].db_socket, &fd_socket_set);
            if (raw_interfaces_status[i].db_socket > max_sd)
                max_sd = raw_interfaces_status[i].db_socket;
        }
        //add child sockets (tcp connection sockets) to set
        for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
            if (tcp_clients[i] > 0) {
                FD_SET(tcp_clients[i], &fd_socket_set);
                if (tcp_clients[i] > max_sd)
                    max_sd = tcp_clients[i];
            }
        }

        select_return = select(max_sd + 1, &fd_socket_set, NULL, NULL, &socket_timeout);
        if (select_return == -1) {
            perror("DB_STATUS_GROUND: select() returned error: ");
        } else if (select_return > 0) {
            for (int i = 0; i < num_inf_status; ++i) {
                if (FD_ISSET(raw_interfaces_status[i].db_socket, &fd_socket_set)) {
                    // ---------------
                    // status message from long range link (UAV)
                    // ---------------
                    l = recv(raw_interfaces_status[i].db_socket, lr_buffer, DATA_UNI_LENGTH, 0);
                    if (l > 0) {
                        get_db_payload(lr_buffer, l, message_buff, &seq_num_status, &radiotap_length);
                        if (prev_seq_num_status != seq_num_status) {
                            prev_seq_num_status = seq_num_status;
                            // process payload (currently only one type of raw status frame is supported: RC_AIR --> STATUS_GROUND)
                            // must be a uav_rc_status_update_message_t
                            struct uav_rc_status_update_message_t *rc_status_message = (struct uav_rc_status_update_message_t *) message_buff;
                            db_sys_status_message.rssi_drone = rc_status_message->rssi_rc_uav;
                            db_sys_status_message.recv_pack_sec = rc_status_message->recv_pack_sec;
                            db_rc_status_t->adapter[0].current_signal_dbm = db_sys_status_message.rssi_drone;
                            db_rc_status_t->received_packet_cnt = db_sys_status_message.recv_pack_sec;
                            db_uav_status->cpuload = rc_status_message->cpu_usage_uav;
                            db_uav_status->temp = rc_status_message->cpu_temp_uav;
                            db_uav_status->undervolt = rc_status_message->uav_is_low_V;
                        }
                    }
                }
            }

            // handle incoming tcp connection requests on master TCP socket
            if (FD_ISSET(status_tcp_server_info.sock_fd, &fd_socket_set)) {
                if ((new_tcp_client = accept(status_tcp_server_info.sock_fd,
                                             (struct sockaddr *) &status_tcp_server_info.servaddr,
                                             (socklen_t *) &tcp_addrlen)) < 0) {
                    perror("DB_STATUS_GND: Accepting new tcp connection failed");
                } else {
                    //add new socket to array of sockets
                    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                        if (tcp_clients[i] == 0) {   // if position is empty
                            tcp_clients[i] = new_tcp_client;
                            LOG_SYS_STD(LOG_INFO, "DB_STATUS_GND: New connection (%s:%d)\n",
                                        inet_ntoa(status_tcp_server_info.servaddr.sin_addr),
                                        ntohs(status_tcp_server_info.servaddr.sin_port));
                            break;
                        }
                    }
                }
            }
            // handle messages from connected TCP clients
            for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                if (FD_ISSET(tcp_clients[i], &fd_socket_set)) {
                    if (read(tcp_clients[i], tcp_message_buff, NET_BUFF_SIZE) == 0) {
                        //Somebody disconnected , get his details and print
                        getpeername(tcp_clients[i], (struct sockaddr *) &status_tcp_server_info.servaddr,
                                    (socklen_t *) &tcp_addrlen);
                        LOG_SYS_STD(LOG_INFO, "DB_STATUS_GND: Client disconnected (%s:%d)\n",
                               inet_ntoa(status_tcp_server_info.servaddr.sin_addr),
                               ntohs(status_tcp_server_info.servaddr.sin_port));
                        close(tcp_clients[i]);
                        tcp_clients[i] = 0;
                    } else {
                        // client sent us some information. Process it...
                        switch (tcp_message_buff[2]) {
                            case 0x03:
                                // DB RC overwrite message. Set overwrite values in shared memory
                                memcpy(rc_overwrite_values->ch, &tcp_message_buff[3], (size_t) 2 * NUM_CHANNELS);
                                clock_gettime(CLOCK_MONOTONIC_COARSE, &timestamp);
                                rc_overwrite_values->timestamp = timestamp;
                                break;
                            default:
                                LOG_SYS_STD(LOG_WARNING, "DB_STATUS_GND: Unknown status message received from GCS\n");
                                break;
                        }
                    }
                }
            }
        }

        // status messages for the ground control station (app)
        // sent at 10Hz independently of whether data was received from UAV or not
        gettimeofday(&timecheck, NULL);
        rightnow = (long) timecheck.tv_sec * 1000 + (long) timecheck.tv_usec / 1000;
        if ((rightnow - start) >= status_message_update_rate) {
            // ---------------
            // DB system-status message
            // ---------------
            best_dbm = -128;
            for (cardcounter = 0; cardcounter < number_cards; ++cardcounter) {
                if (best_dbm < db_gnd_status_t->adapter[cardcounter].current_signal_dbm)
                    best_dbm = db_gnd_status_t->adapter[cardcounter].current_signal_dbm;
            }
            db_uav_status->undervolt = get_undervolt();
            db_sys_status_message.rssi_ground = best_dbm;
            db_sys_status_message.damaged_blocks_wbc = db_gnd_status_t->damaged_block_cnt;
            db_sys_status_message.lost_packets_wbc = db_gnd_status_t->lost_packet_cnt;
            db_sys_status_message.kbitrate_wbc = db_gnd_status_t->kbitrate;
            db_sys_status_message.voltage_status = db_uav_status->undervolt;
            if (db_gnd_status_t->tx_restart_cnt > restarts) {
                restarts++;
                usleep((__useconds_t) 1e7);
            }
            // ---------------
            // send DB system status message
            // ---------------
            send_to_all_tcp_clients(tcp_clients, (uint8_t *) &db_sys_status_message, sizeof(db_system_status_msg_t));
            // ---------------
            // send DB RC-status message
            // ---------------
            memcpy(db_rc_status_message.channels, rc_values->ch, 2 * NUM_CHANNELS);
            send_to_all_tcp_clients(tcp_clients, (uint8_t *) &db_rc_status_message, sizeof(db_rc_msg_t));

            gettimeofday(&timecheck, NULL);
            start = (long) timecheck.tv_sec * 1000 + (long) timecheck.tv_usec / 1000;
        }
    }
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (tcp_clients[i] > 0)
            close(tcp_clients[i]);
    }
    close(status_tcp_server_info.sock_fd);
    for (int i = 0; i < DB_MAX_ADAPTERS; i++) {
        if (raw_interfaces_status[i].db_socket > 0)
            close(raw_interfaces_status[i].db_socket);
    }
    LOG_SYS_STD(LOG_INFO, "DB_STATUS_GND: Terminated!\n");
    exit(0);
}