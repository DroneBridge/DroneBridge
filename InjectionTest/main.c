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
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

#include "../common/db_raw_send_receive.h"
#include "../common/db_raw_receive.h"

#define UNIX_PATH   "/tmp/endpoint"


int keep_running = 1;

void int_handler(int dummy) {
    keep_running = 0;
}

int open_configure_unix_socket() {
    struct sockaddr_un sock_server_add;
    memset(&sock_server_add, 0x00, sizeof(sock_server_add));

    int unix_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_sock < 0) {
        perror("DB_USB: Error opening datagram socket");
        exit(-1);
    }
    sock_server_add.sun_family = AF_UNIX;
    strcpy(sock_server_add.sun_path, UNIX_PATH);
    unlink(UNIX_PATH);
    if (bind(unix_sock, (struct sockaddr *)&sock_server_add, SUN_LEN(&sock_server_add)) < 0) {
        perror("DB_USB: Error binding name to datagram socket");
        close(unix_sock);
        exit(-1);
    }
    printf("Opened UNIX socket\n");
    return unix_sock;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    uint8_t buff[4096];
    struct sockaddr_un peer_sock;

    // -----------
    int unix_client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_client_socket < 0) {
        printf("DB_VIDEO_GND: Failed opening UNIX domain socket\n");
        exit(-1);
    }
    struct sockaddr_un unix_socket_addr;
    unix_client_socket = set_socket_nonblocking(unix_client_socket);
    memset(&unix_socket_addr, 0x00, sizeof(unix_socket_addr));
    unix_socket_addr.sun_family = AF_UNIX;
    strcpy(unix_socket_addr.sun_path, UNIX_PATH);
    socklen_t server_length = sizeof(struct sockaddr_un);
    // ---------------

    int unix_server_socket = open_configure_unix_socket();

    while(keep_running) {
        if(sendto(unix_client_socket, buff, 50, 0, (struct sockaddr *) &unix_socket_addr, server_length) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                printf("DB_VIDEO_GND: Error sending via UNIX domain socket - %s\n", strerror(errno));
            else
                printf("DB_VIDEO_GND: Error sending to unix domain - might lost a packet\n");
        }
        ssize_t tcp_num_recv = recv(unix_server_socket, buff, 4096, 0);
        if (tcp_num_recv > 0) {
            printf("Received %zi\n", tcp_num_recv);
        }
        sleep(1);
    }
    unlink(UNIX_PATH);

/*    char interface[IFNAMSIZ];
    strcpy(interface, argv[1]);

    db_socket_t socket = open_db_socket(interface, 16, 'm', 11, DB_DIREC_GROUND, 10, (uint8_t) atoi(argv[2]));

    uint8_t payload[payload_size];
    memset(payload, 1, payload_size);

    for (int i = 0; i < 100; ++i) {
        send_packet_div(&socket, payload, 10, payload_size, 66, 1);
        printf(".");
        fflush(stdout);
        usleep((unsigned int) 1e6);
    }*/
    printf("Terminated\n");
}
#pragma clang diagnostic pop