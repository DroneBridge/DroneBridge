/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   parts based on rx_status by Rodizio. Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
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
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include "../common/wbc_lib.h"
#include "../common/db_get_ip.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/shared_memory.h"
#include "../common/ccolors.h"
#include "../common/db_utils.h"

#define UDP_STATUS_BUFF_SIZE 2048

bool volatile keeprunning = true;
char if_name_telemetry[IFNAMSIZ];
char db_mode;
uint8_t comm_id = DEFAULT_V2_COMMID;
int c, app_port_status = APP_PORT_STATUS;
float rc_send_rate = 60; // [packets/s]

void intHandler(int dummy)
{
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]){
    strncpy(if_name_telemetry, DEFAULT_DB_IF, IFNAMSIZ);
    db_mode = DEFAULT_DB_MODE;
    app_port_status = APP_PORT_STATUS;
    opterr = 0;
    while ((c = getopt (argc, argv, "n:m:c:p:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(if_name_telemetry, optarg, IFNAMSIZ);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'p':
                app_port_status = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("This tool sends extra information about the video stream and RC via UDP to IP given by "
                               "IP-checker module. Use"
                               "\n\t-n <network_IF> "
                               "\n\t-m [w|m] default is <m>"
                               "\n\t-p Specify a UDP port to which we send the status information. IP comes from IP "
                               "checker module. Default:%i"
                               "\n\t-c <communication id> Choose a number from 0-255. Same on groundstation and drone!"
                        , APP_PORT_STATUS);
                break;
            default:
                abort ();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    struct timespec timestamp;
    int restarts = 0, udp_status_socket, shID, cardcounter = 0, select_return, radiotap_length, max_sd;
    struct timeval timecheck;
    long start, rightnow, status_message_update_rate = 100; // send status messages every 100ms (10Hz)
    int8_t best_dbm = 0;
    ssize_t l;
    uint8_t counter = 0;
    uint8_t lr_buffer[DATA_UNI_LENGTH];
    uint8_t udp_status_buffer[UDP_STATUS_BUFF_SIZE];

    DB_RC_MESSAGE db_rc_status_message;
    db_rc_status_message.ident[0] = '$'; db_rc_status_message.ident[1] = 'D'; db_rc_status_message.message_id = 2;
    DB_SYSTEM_STATUS_MESSAGE db_sys_status_message;
    db_sys_status_message.ident[0] = '$'; db_sys_status_message.ident[1] = 'D'; db_sys_status_message.message_id = 1;
    db_sys_status_message.mode = 'm'; db_sys_status_message.packetloss_rc = 0; db_sys_status_message.rssi_drone = 0;
    db_sys_status_message.voltage_status = 0;

    process_command_line_args(argc, argv);

    // open wbc rc shared memory to push rc rssi etc. to wbc OSD
    wifibroadcast_rx_status_t_rc *wbc_rc_status = wbc_rc_status_memory_open();
    // open db rc shared memory
    db_rc_values *rc_values = db_rc_values_memory_open();
    // open db rc overwrite shared memory
    db_rc_overwrite_values *rc_overwrite_values = db_rc_overwrite_values_memory_open();
    // open wbc rx status shared memory
    wifibroadcast_rx_status_t *wbc_rx_status = wbc_status_memory_open();
    // open wbc air sys status shared memory - gets read by OSD
    wifibroadcast_rx_status_t_sysair *wbc_sys_air_status = wbc_sysair_status_memory_open();

    int number_cards = wbc_rx_status->wifi_adapter_cnt;

    // set up long range receiving socket
    int long_range_socket = open_receive_socket(if_name_telemetry, db_mode, comm_id, DB_DIREC_GROUND, DB_PORT_STATUS);
    long_range_socket = set_socket_nonblocking(long_range_socket);
    fd_set fd_socket_set;
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 100000; // 10Hz

    // set up UDP status socket & dest. address for status messages (remoteServAddr)
    struct sockaddr_in remoteServAddr, udp_status_addr, client_status_addr;
    socklen_t client_status_addr_len = sizeof(client_status_addr);
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_addr.s_addr = inet_addr("192.168.2.2");
    remoteServAddr.sin_port = htons(app_port_status);
    udp_status_addr.sin_family = AF_INET;
    udp_status_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_status_addr.sin_port = htons(app_port_status);
    udp_status_socket = socket (AF_INET, SOCK_DGRAM, 0);
    const int y = 1;
    setsockopt(udp_status_socket, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
    if (udp_status_socket < 0) {
        printf (RED "DB_STATUS_GROUND: %s: Unable to open status socket" RESET "\n", strerror(errno));
        exit (EXIT_FAILURE);
    }
    int broadcast=1;
    if (setsockopt(udp_status_socket, SOL_SOCKET, SO_BROADCAST, &broadcast,sizeof(broadcast))==-1) {
        printf(RED "DB_STATUS_GROUND: %s" RESET "\n",strerror(errno));
    }
    if (bind(udp_status_socket, (struct sockaddr *) &udp_status_addr, sizeof (udp_status_addr)) < 0) {
        printf(RED "DB_PROXY_GROUND: Unable to bind to port %i (%s)\n" RESET, app_port_status, strerror(errno));
        exit (EXIT_FAILURE);
    }

    // get IP shared memory ID
    shID = init_shared_memory_ip();

    printf(GRN "DB_STATUS_GROUND: started!" RESET "\n");

    gettimeofday(&timecheck, NULL);
    start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
    while(keeprunning) {
        socket_timeout.tv_sec = 0;
        socket_timeout.tv_usec = 100000; // 10Hz
        FD_ZERO (&fd_socket_set);
        FD_SET (long_range_socket, &fd_socket_set);
        FD_SET (udp_status_socket, &fd_socket_set);
        max_sd = long_range_socket;
        if (udp_status_socket > max_sd)
        {
            max_sd = udp_status_socket;
        }
        select_return = select (max_sd+1, &fd_socket_set, NULL, NULL, &socket_timeout);
        // ---------------
        // Get IP from IP Checker shared memory segment 10th time
        // ---------------
        counter++;
        if (counter>9){
            counter = 0;
            remoteServAddr.sin_addr.s_addr = inet_addr(get_ip_from_ipchecker(shID));
        }

        if(select_return == -1)
        {
            perror("DB_STATUS_GROUND: select() returned error: ");
        }else if (select_return > 0){
            if (FD_ISSET(long_range_socket, &fd_socket_set)){
                // ---------------
                // status message from long range link (UAV)
                // ---------------
                l = recv(long_range_socket, lr_buffer, DATA_UNI_LENGTH, 0);
                if (l > 0){
                    // get payload
                    radiotap_length = lr_buffer[2] | (lr_buffer[3] << 8);
                    // message_length = lr_buffer[radiotap_length+19] | (lr_buffer[radiotap_length+20] << 8);
                    // process payload (currently only one type of raw status frame is supported: RC_AIR --> STATUS_GROUND)
                    // must be a uav_rc_status_update_message
                    db_sys_status_message.rssi_drone = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH];
                    db_sys_status_message.packetloss_rc = (uint8_t) (
                            (1 - (lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 1] / rc_send_rate)) * 100);
                    wbc_rc_status->adapter[0].current_signal_dbm = db_sys_status_message.rssi_drone;
                    wbc_rc_status->lost_packet_cnt = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 1];
                    wbc_sys_air_status->cpuload = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 2];
                    wbc_sys_air_status->temp = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 3];
                    wbc_sys_air_status->undervolt = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 4];
                }
            }

            if (FD_ISSET(udp_status_socket, &fd_socket_set)){
                // ---------------
                // status message from ground control station (app)
                // ---------------
                l = recvfrom(udp_status_socket, udp_status_buffer, UDP_STATUS_BUFF_SIZE, 0,
                             (struct sockaddr *)&client_status_addr, &client_status_addr_len);
                int err = errno;
                if (l > 0){
                    switch (udp_status_buffer[2]) {
                        case 0x03:
                            // DB RC overwrite message
                            memcpy(rc_overwrite_values->ch, &udp_status_buffer[3], (size_t) 2*NUM_CHANNELS);
                            clock_gettime(CLOCK_MONOTONIC_COARSE, &timestamp);
                            rc_overwrite_values->timestamp = timestamp;
                            break;
                        default:
                            printf(RED "Unknown status message received from GCS" RESET "\n");
                            break;
                    }
                } else {
                    printf(RED "DB_STATUS_GROUND: UDP socket received an error: %s" RESET "\n", strerror(err));
                }
            }
        }

        // status messages for the ground control station (app)
        // sent at 10Hz independently of whether data was received from UAV or not
        gettimeofday(&timecheck, NULL);
        rightnow = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        if ((rightnow-start) >= status_message_update_rate){
            // ---------------
            // DB system-status message
            // ---------------
            best_dbm = -128;
            for(cardcounter=0; cardcounter<number_cards; ++cardcounter) {
                if (best_dbm < wbc_rx_status->adapter[cardcounter].current_signal_dbm)
                    best_dbm = wbc_rx_status->adapter[cardcounter].current_signal_dbm;
            }
            db_sys_status_message.rssi_ground = best_dbm;
            db_sys_status_message.damaged_blocks_wbc = wbc_rx_status->damaged_block_cnt;
            db_sys_status_message.lost_packets_wbc = wbc_rx_status->lost_packet_cnt;
            db_sys_status_message.kbitrate_wbc = wbc_rx_status-> kbitrate;
            db_sys_status_message.voltage_status = ((wbc_sys_air_status->undervolt << 1) | get_undervolt());
            if (wbc_rx_status->tx_restart_cnt > restarts) {
                restarts++;
                usleep ((__useconds_t) 1e7);
            }
            sendto (udp_status_socket, &db_sys_status_message, sizeof(DB_SYSTEM_STATUS_MESSAGE), 0, (struct sockaddr *) &remoteServAddr,
                    sizeof (remoteServAddr));

            // ---------------
            // DB RC-status message
            // ---------------
            memcpy(db_rc_status_message.channels, rc_values->ch, 2*NUM_CHANNELS);
            sendto (udp_status_socket, &db_rc_status_message, sizeof(DB_RC_MESSAGE), 0, (struct sockaddr *) &remoteServAddr,
                    sizeof (remoteServAddr));

            gettimeofday(&timecheck, NULL);
            start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        }
    }
    close(udp_status_socket);
    close(long_range_socket);
    return 0;
}