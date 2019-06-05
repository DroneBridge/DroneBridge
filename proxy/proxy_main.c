/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2018 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zconf.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include "../common/db_get_ip.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/db_raw_send_receive.h"
#include "../common/ccolors.h"
#include "../common/tcp_server.h"

#define TCP_BUFFER_SIZE (DATA_UNI_LENGTH-DB_RAW_V2_HEADER_LENGTH)

bool volatile keeprunning = true;
char db_mode, write_to_osdfifo;
uint8_t comm_id = DEFAULT_V2_COMMID, frame_type;
int bitrate_op, prox_adhere_80211, num_interfaces;
char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];

void intHandler(int dummy) {
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]) {
    db_mode = DEFAULT_DB_MODE;
    write_to_osdfifo = 'Y';
    opterr = 0;
    num_interfaces = 0;
    bitrate_op = 1;
    prox_adhere_80211 = 0;
    frame_type = DB_FRAMETYPE_DEFAULT;
    int c;
    while ((c = getopt(argc, argv, "n:m:c:b:o:f:a:?")) != -1) {
        switch (c) {
            case 'n':
                if (num_interfaces < DB_MAX_ADAPTERS) {
                    strncpy(adapters[num_interfaces], optarg, IFNAMSIZ);
                    num_interfaces++;
                }
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'o':
                write_to_osdfifo = *optarg;
                break;
            case 'f':
                frame_type = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'a':
                prox_adhere_80211 = (int) strtol(optarg, NULL, 10);
            case '?':
                printf("DroneBridge Proxy module is used to do any UDP <-> DB_CONTROL_AIR routing. UDP IP given by "
                       "IP-checker module. Use"
                       "\n\t-n <network_IF_proxy_module> "
                       "\n\t-m [w|m] DroneBridge mode - wifi - monitor mode (default: m) (wifi not supported yet!)"
                       "\n\t-c <communication id> Choose a number from 0-255. Same on groundstation and drone!"
                       "\n\t-o [Y|N] Write telemetry to /root/telemetryfifo1 FIFO (default: Y)"
                       "\n\t-f <1|2> DroneBridge v2 raw protocol packet/frame type: 1=RTS, 2=DATA (CTS protection)"
                       "\n\t-b bit rate:\tin Mbps (1|2|5|6|9|11|12|18|24|36|48|54)\n\t\t(bitrate option only "
                       "supported with Ralink chipsets)"
                       "\n\t-a <0|1> to disable/enable. Offsets the payload by some bytes so that it sits outside "
                       "then 802.11 header. Set this to 1 if you are using a non DB-Rasp Kernel!");
                break;
            default:
                abort();
        }
    }
}

int open_osd_fifo() {
    int tempfifo_osd = open("/root/telemetryfifo1", O_WRONLY | O_NONBLOCK);
    if (tempfifo_osd == -1) {
        printf(YEL "DB_PROXY_GROUND: Unable to open OSD FIFO\n" RESET);
    }
    return tempfifo_osd;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    signal(SIGPIPE, SIG_IGN);
    usleep((__useconds_t) 1e6);
    process_command_line_args(argc, argv);

    // set up long range sockets
    db_socket raw_interfaces[DB_MAX_ADAPTERS] = {0};
    for (int i = 0; i < num_interfaces; ++i) {
        raw_interfaces[i] = open_db_socket(adapters[i], comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_PROXY,
                                           frame_type);
    }
    int fifo_osd = -1, max_clients = 10, tcp_clients[max_clients], new_tcp_client;
    if (write_to_osdfifo == 'Y') {
        fifo_osd = open_osd_fifo();
    }

    // init variables
    uint16_t radiotap_length = 0;
    fd_set fd_socket_set;
    struct timeval select_timeout;
    size_t recv_length = 0;

    // Setup TCP server for GCS communication
    struct tcp_server_info tcp_server_info = create_tcp_server_socket(APP_PORT_PROXY);
    int tcp_addrlen = sizeof(tcp_server_info.servaddr);

    struct data_uni *data_uni_to_drone = get_hp_raw_buffer(prox_adhere_80211);
    uint8_t seq_num = 0, seq_num_proxy = 0, last_recv_seq_num = 0;
    uint8_t lr_buffer[DATA_UNI_LENGTH];
    uint8_t tcp_buffer[TCP_BUFFER_SIZE];
    size_t payload_length = 0;

    printf(GRN "DB_PROXY_GROUND: started! Enabled diversity on %i adapters." RESET "\n", num_interfaces);
    while (keeprunning) {
        select_timeout.tv_sec = 5;
        select_timeout.tv_usec = 0;
        FD_ZERO (&fd_socket_set);
        FD_SET (tcp_server_info.sock_fd, &fd_socket_set);
        int max_sd = tcp_server_info.sock_fd;
        // add raw DroneBridge sockets
        for (int i = 0; i < num_interfaces; i++) {
            FD_SET (raw_interfaces[i].db_socket, &fd_socket_set);
            if (raw_interfaces[i].db_socket > max_sd)
                max_sd = raw_interfaces[i].db_socket;
        }
        // add child sockets (tcp connection sockets) to set
        for (int i = 0; i < max_clients; i++) {
            int tcp_client_sd = tcp_clients[i];
            if (tcp_client_sd > 0)
                FD_SET(tcp_client_sd, &fd_socket_set);
            if (tcp_client_sd > max_sd)
                max_sd = tcp_client_sd;
        }

        int select_return = select(max_sd + 1, &fd_socket_set, NULL, NULL, &select_timeout);
        if (select_return == -1) {
            perror("DB_PROXY_GROUND: select() returned error: ");
        } else if (select_return > 0) {
            for (int i = 0; i < num_interfaces; i++) {
                if (FD_ISSET(raw_interfaces[i].db_socket, &fd_socket_set)) {
                    // ---------------
                    // incoming form long range proxy port - write data to OSD-FIFO and pass on to connected TCP clients
                    // ---------------
                    ssize_t l = recv(raw_interfaces[i].db_socket, lr_buffer, DATA_UNI_LENGTH, 0);
                    int err = errno;
                    if (l > 0) {
                        payload_length = get_db_payload(lr_buffer, l, tcp_buffer, &seq_num_proxy, &radiotap_length);
                        if (seq_num_proxy != last_recv_seq_num) {
                            last_recv_seq_num = seq_num_proxy;
                            send_to_all_tcp_clients(tcp_clients, tcp_buffer, payload_length);
                            if (fifo_osd != -1 && write_to_osdfifo == 'Y') {
                                ssize_t written = write(fifo_osd, tcp_buffer, payload_length);
                                if (written < 1)
                                    perror(RED "DB_TEL_GROUND: Could not write to OSD FIFO" RESET);
                            } else if (write_to_osdfifo == 'Y')
                                printf(YEL "DB_PROXY_GROUND: No OSD-FIFO open. Trying to reopen!\n" RESET);
                        }
                    } else
                        printf(RED "DB_PROXY_GROUND: Long range socket received an error: %s\n" RESET, strerror(err));
                }
            }
            // handle incoming tcp connection requests on master TCP socket
            if (FD_ISSET(tcp_server_info.sock_fd, &fd_socket_set)) {
                if ((new_tcp_client = accept(tcp_server_info.sock_fd,
                                             (struct sockaddr *) &tcp_server_info.servaddr,
                                             (socklen_t *) &tcp_addrlen)) < 0) {
                    perror("DB_PROXY_GROUND: Accepting new tcp connection failed");
                }
                printf("DB_PROXY_GROUND: New connection (%s:%d)\n", inet_ntoa(tcp_server_info.servaddr.sin_addr),
                       ntohs(tcp_server_info.servaddr.sin_port));
                //add new socket to array of sockets
                for (int i = 0; i < max_clients; i++) {
                    if (tcp_clients[i] == 0) {   // if position is empty
                        tcp_clients[i] = new_tcp_client;
                        break;
                    }
                }
            }
            // handle messages from connected TCP clients
            for (int i = 0; i < max_clients; i++) {
                int current_client_sock = tcp_clients[i];
                if (FD_ISSET(current_client_sock, &fd_socket_set)) {
                    if ((recv_length = read(current_client_sock, tcp_buffer, TCP_BUFFER_SIZE)) == 0) {
                        //Somebody disconnected , get his details and print
                        getpeername(current_client_sock, (struct sockaddr *) &tcp_server_info.servaddr,
                                    (socklen_t *) &tcp_addrlen);
                        printf("DB_PROXY_GROUND: Client disconnected (%s:%d)\n",
                               inet_ntoa(tcp_server_info.servaddr.sin_addr),
                               ntohs(tcp_server_info.servaddr.sin_port));
                        close(current_client_sock);
                        tcp_clients[i] = 0;
                    } else {
                        // client sent us some information. Process it...
                        memcpy(data_uni_to_drone->bytes, tcp_buffer, recv_length);
                        for (int j = 0; j < num_interfaces; j++)
                            send_packet_hp_div(&raw_interfaces[j], DB_PORT_CONTROLLER, (u_int16_t) recv_length,
                                               update_seq_num(&seq_num));
                    }
                }
            }
        }
    }
    for (int i = 0; i < DB_MAX_ADAPTERS; i++) {
        if (raw_interfaces[i].db_socket > 0)
            close(raw_interfaces[i].db_socket);
    }
    for (int i = 0; i < max_clients; i++) {
        if (tcp_clients[i] > 0)
            close(tcp_clients[i]);
    }
    close(tcp_server_info.sock_fd);
    close(fifo_osd);
    return 0;
}