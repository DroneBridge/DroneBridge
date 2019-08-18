/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2019 Wolfgang Christl
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

/*
 * Creates a UDP server (514) & TCP server (1604). Forwards all UDP traffic to all connected TCP clients.
 * Used to distribute syslog messages received via UDP from rsyslog to clients for debugging
 */

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <zconf.h>
#include <sys/socket.h>
#include <stdint.h>
#include "../common/db_common.h"
#include "../common/tcp_server.h"

#define NET_BUFF_SIZE 2048
#define MAX_TCP_CLIENTS 10
#define PORT_TCP_SYSLOG_SERVER 1604
#define PORT_UDP_SYSLOG_SERVER 514

int keep_running = 1;


void signal_handler(int s) {
    keep_running = 0;
}

int open_udp_server() {
    const int y = 1;
    struct sockaddr_in servAddr;
    int log_udp_socket;
    if ((log_udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("DB_SYSLOG_SERVER: Could not open UDP socket");
        return -1;
    }
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(PORT_UDP_SYSLOG_SERVER);
    setsockopt(log_udp_socket, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
    if (bind(log_udp_socket, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("DB_SYSLOG_SERVER: Could not bind UDP socket to port. Need to be root for this");
        return -1;
    }
    LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: Listening for log messages on UDP port %i", PORT_UDP_SYSLOG_SERVER);
    return log_udp_socket;
}

int main(int argc, char *argv[]) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    uint8_t net_message_buff[NET_BUFF_SIZE];
    memset(net_message_buff, 0, NET_BUFF_SIZE);

    int tcp_clients[MAX_TCP_CLIENTS] = {0};
    fd_set fd_read_set;
    struct tcp_server_info_t tcp_server_syslog = create_tcp_server_socket(PORT_TCP_SYSLOG_SERVER);
    int tcp_addrlen = sizeof(tcp_server_syslog.servaddr);

    struct sockaddr_in udp_client_addr;
    int udp_socket = open_udp_server();
    if (udp_socket < 0) {
        exit(-1);
    } else
        LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: Listening for TCP clients to forward logs on port %i",
                    PORT_TCP_SYSLOG_SERVER);
    int max_sd = 0;

    LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: Started");
    while (keep_running) {
        FD_ZERO(&fd_read_set);
        FD_SET(udp_socket, &fd_read_set);
        max_sd = udp_socket;
        FD_SET(tcp_server_syslog.sock_fd, &fd_read_set);
        //add child sockets (tcp connection sockets) to set
        for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
            int tcp_client_sd = tcp_clients[i];
            if (tcp_client_sd > 0)
                FD_SET(tcp_client_sd, &fd_read_set);
            if (tcp_client_sd > max_sd)
                max_sd = tcp_client_sd;
        }

        int select_return = select(max_sd + 1, &fd_read_set, NULL, NULL, NULL);
        if (select_return > 0) {
            // handle incoming log messages & forward them to connected TCP clients
            if (FD_ISSET(udp_socket, &fd_read_set)) {
                ssize_t recv_bytes = recvfrom(udp_socket, net_message_buff, NET_BUFF_SIZE, 0,
                                              (struct sockaddr *) &udp_client_addr,
                                              (socklen_t *) sizeof(udp_client_addr));
                if (recv_bytes > 0) {
                    send_to_all_tcp_clients(tcp_clients, net_message_buff, NET_BUFF_SIZE);
                }
            }
            // handle incoming tcp connection requests on master TCP socket
            if (FD_ISSET(tcp_server_syslog.sock_fd, &fd_read_set)) {
                int new_tcp_client;
                if ((new_tcp_client = accept(tcp_server_syslog.sock_fd,
                                             (struct sockaddr *) &tcp_server_syslog.servaddr,
                                             (socklen_t *) &tcp_addrlen)) < 0) {
                    perror("DB_SYSLOG_SERVER: Accepting new tcp connection failed");
                } else {
                    LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: New connection (%s:%d)\n",
                                inet_ntoa(tcp_server_syslog.servaddr.sin_addr),
                                ntohs(tcp_server_syslog.servaddr.sin_port));
                    //add new socket to array of sockets
                    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                        if (tcp_clients[i] == 0) {   // if position is empty
                            tcp_clients[i] = new_tcp_client;
                            break;
                        }
                    }
                }
            }
            // handle messages from connected TCP clients
            for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                if (FD_ISSET(tcp_clients[i], &fd_read_set)) {
                    if (read(tcp_clients[i], net_message_buff, NET_BUFF_SIZE) == 0) {
                        //Somebody disconnected
                        getpeername(tcp_clients[i], (struct sockaddr *) &tcp_server_syslog.servaddr,
                                    (socklen_t *) &tcp_addrlen);
                        LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: Client disconnected (%s:%d)\n",
                                    inet_ntoa(tcp_server_syslog.servaddr.sin_addr),
                                    ntohs(tcp_server_syslog.servaddr.sin_port));
                        close(tcp_clients[i]);
                        tcp_clients[i] = 0;
                    } else {
                        // tcp client sent us some information. Process it...
                        LOG_SYS_STD(LOG_WARNING, "DB_SYSLOG_SERVER: TCP server is not accepting any data\n");
                    }
                }
            }
        } else if (select_return == -1)
            perror("DB_SYSLOG_SERVER: select returned error");
    }

    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (tcp_clients[i] > 0)
            close(tcp_clients[i]);
    }
    close(tcp_server_syslog.sock_fd);
    LOG_SYS_STD(LOG_INFO, "DB_SYSLOG_SERVER: Terminated");
    exit(0);
}
