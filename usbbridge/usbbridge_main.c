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

// requires read/write permission on usb udev device with vendor id: 0x18D1 (Google) -> edit udev rules accordingly
// https://unix.stackexchange.com/questions/44308/understanding-udev-rules-and-permissions-in-libusb

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <zconf.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include "linux_aoa.h"
#include "../common/db_protocol.h"


bool keeprunning = true;

#define DATA_LENGTH     256
#define TCP_BUFF_SIZ    4096

void intHandler(int dummy) {
    keeprunning = false;
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
    strcpy(sock_server_add.sun_path, DB_UNIX_DOMAIN_VIDEO_PATH);
    unlink(DB_UNIX_DOMAIN_VIDEO_PATH);
    if (bind(unix_sock, (struct sockaddr *)&sock_server_add, SUN_LEN(&sock_server_add))) {
        perror("DB_USB: Error binding name to datagram socket");
        close(unix_sock);
        exit(-1);
    }
    return unix_sock;
}

int open_local_tcp_socket(int port) {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("DB_USB: Error socket creation failed");
        exit(-1);
    }
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        perror("DB_USB: Error setting reusable");
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        perror("DB_USB: Error connection with the server failed");
        close(sockfd);
        exit(-1);
    }
    return sockfd;
}

int main(int argc, char *argv[]) {
    int video_unix_socket = open_configure_unix_socket();
    int proxy_sock = open_local_tcp_socket(APP_PORT_PROXY);
    int status_sock = open_local_tcp_socket(APP_PORT_STATUS);
    int communication_sock = open_local_tcp_socket(APP_PORT_COMM);

    db_accessory_t accessory;
    init_db_accessory(&accessory); // blocking
    struct timeval tv;
//    struct libusb_pollfd usb_fds = **libusb_get_pollfds(NULL);
//    struct pollfd fds[5];
//    memset(fds, 0 , sizeof(fds));
//    fds[0].fd = video_unix_socket;
//    fds[0].events = POLLIN;
//    fds[1].fd = proxy_sock;
//    fds[1].events = POLLIN;
//    fds[2].fd = status_sock;
//    fds[2].events = POLLIN;
//    fds[3].fd = communication_sock;
//    fds[3].events = POLLIN;

    db_usb_msg_t *usb_msg = db_usb_get_direct_buffer();
    uint8_t tcp_buffer[TCP_BUFF_SIZ];

    struct timeval time;
    gettimeofday(&time, NULL);
    long start = (long) time.tv_sec * 1000 + (long) time.tv_usec / 1000;
    long rightnow;

    // TODO: use async libusb API
    fd_set readset; ssize_t tcp_received;
    while (keeprunning) {
        tv.tv_sec = 0; tv.tv_usec = 500;
        FD_ZERO(&readset);
        int max_sd = video_unix_socket;
        FD_SET(video_unix_socket, &readset);

        FD_SET(proxy_sock, &readset);
        if (proxy_sock > max_sd)
            max_sd = proxy_sock;

        FD_SET(status_sock, &readset);
        if (status_sock > max_sd)
            max_sd = status_sock;

        FD_SET(communication_sock, &readset);
        if (communication_sock > max_sd)
            max_sd = communication_sock;

        int select_return = select(max_sd + 1, &readset, NULL, NULL, &tv);
        if (select_return == -1) {
            perror("DB_VIDEO_GND: select() returned error: ");
        } else if (select_return > 0) {
            if (FD_ISSET(video_unix_socket, &readset)) {
                tcp_received = recv(video_unix_socket, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_VIDEO);
            }
            if (FD_ISSET(proxy_sock, &readset)) {
                tcp_received = recv(proxy_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_PROXY);
            }
            if (FD_ISSET(status_sock, &readset)) {
                tcp_received = recv(status_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_STATUS);
            }
            if (FD_ISSET(communication_sock, &readset)) {
                tcp_received = recv(communication_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_COMM);
            }
        } else {
            // timeout
            // TODO send ping to phone to get it out of the read loop to be able to write stuff
        }
        gettimeofday(&time, NULL);
        rightnow = (long) time.tv_sec * 1000 + (long) time.tv_usec / 1000;
        if ((rightnow - start) >= 200) {
            db_usb_receive(&accessory, tcp_buffer, TCP_BUFF_SIZ, 1);
            // TODO: parse message and process it
            start = (long) time.tv_sec * 1000 + (long) time.tv_usec / 1000;
        }

//        libusb_get_next_timeout(NULL, &tv);
//        int rc = poll(fds, 5, tv.tv_usec*1000);
//
//        if (poll() indicated activity on libusb file descriptors)
//            libusb_handle_events_timeout(ctx, &zero_tv);
        // handle events from other sources here
    }

// clean up and exit

//    uint8_t data[DATA_LENGTH] = {5};
//    db_usb_msg_t *usb_msg = db_usb_get_direct_buffer();
//    memcpy(usb_msg->payload, data, DATA_LENGTH);
//    usb_msg->pay_lenght = 256;
//    for (int i = 0; i < 100; i++) {
//        // db_usb_send(&accessory, data, DATA_LENGTH, 0x09);
//        db_usb_send_zc(&accessory);
//        // db_usb_send_debug(&accessory);
//        // db_usb_receive_debug(&accessory);
//    }

    exit_close_aoa_device(&accessory);
    close(video_unix_socket);
    close(proxy_sock);
    close(status_sock);
    close(communication_sock);
    unlink(DB_UNIX_DOMAIN_VIDEO_PATH);
    return 0;
}