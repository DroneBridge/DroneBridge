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

#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "tcp_server.h"

/**
 * Open a TCP-Server master socket
 *
 * @param port Port of the TCP Server
 * @return Socket file descriptor
 */
struct tcp_server_info_t create_tcp_server_socket(uint port) {
    int socket_fd;
    struct sockaddr_in servaddr;
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == -1) {
        perror("TCP socket creation failed...");
        exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        perror("DB_TCP_SERVER: setsockopt");
    }

    // Binding newly created socket to given IP and verification
    if ((bind(socket_fd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        perror("TCP socket bind failed...");
    }
    // Allow max 5 pending connections
    if (listen(socket_fd, 5) == -1)
        perror("DB_TCP_SERVER: Error setting TCP socket to listen...");
    struct tcp_server_info_t returnval = {socket_fd, servaddr};
    return returnval;
}

/**
 * Send a message to all clients connected to the TCP server
 *
 * @param list_client_sockets
 * @param message
 * @param message_length
 */
void send_to_all_tcp_clients(const int list_client_sockets[], const uint8_t message[], int message_length) {
    int max_number_clients = sizeof(&list_client_sockets) / sizeof(int);
    for (int i = 0; i < max_number_clients; i++) {
        if (list_client_sockets[i] > 0) {
            if (send(list_client_sockets[i], message, message_length, 0) != message_length) {
                perror("DB_TCP_SERVER: Sending message via TCP");
            }
        }
    }
}