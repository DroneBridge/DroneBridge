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
#include <errno.h>
#include "linux_aoa.h"
#include "../common/db_protocol.h"
#include "../common/ccolors.h"


#define TCP_BUFF_SIZ    4096
#define USB_BUFFER_SIZ  1024

bool keeprunning = true;
bool video_module_activated = false;
bool communication_module_activated = false;
bool proxy_module_activated = false;
bool status_module_activated = false;

uint8_t usb_in_data[USB_BUFFER_SIZ];


void intHandler(int dummy) {
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "v:c:p:s:?")) != -1) {
        switch (c) {
            case 'v':
                if (*optarg == 'Y')
                    video_module_activated = true;
                break;
            case 'c':
                if (*optarg == 'Y')
                    communication_module_activated = true;
                break;
            case 'p':
                if (*optarg == 'Y')
                    proxy_module_activated = true;
                break;
            case 's':
                if (*optarg == 'Y')
                    status_module_activated = true;
                break;
            case '?':
                printf("Transforms the device into an android accessory. Reads data from DroneBridge modules and "
                       "passes it on to the DroneBridge for android app via USB."
                       "\n\t-v Set to Y to listen for video module data"
                       "\n\t-c Set to Y to listen for communication module data"
                       "\n\t-p Set to Y to listen for proxy module data"
                       "\n\t-s Set to Y to listen for status module data");
                break;
            default:
                abort();
        }
    }
    return 0;
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
    bool connected = false;
    while (!connected) {
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
            printf(RED "DB_USB: Error connection with local server on port %i failed: %s"RESET"\n", port, strerror(errno));
            usleep(1e6);
        } else
            connected = true;
    }
    return sockfd;
}

void callback_usb_async_complete(struct libusb_transfer *xfr) {
    switch(xfr->status) {
        case LIBUSB_TRANSFER_COMPLETED:
            printf("Transfered %i!\n", xfr->actual_length);
            // xfr->buffer
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            printf("Transfer cancelled\n");
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            printf("No device!\n");
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
            // printf("Timed out!\n");
            break;
        case LIBUSB_TRANSFER_ERROR:
            printf("Transfer error!\n");
            break;
        case LIBUSB_TRANSFER_STALL:
            printf("Transfer stall!\n");
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            printf("Transfer overflow!\n");
            // Various type of errors here
            break;
    }
    libusb_free_transfer(xfr);
}

void db_read_usb_async(struct accessory_t *accessory) {
    struct libusb_transfer *xfr = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_IN, usb_in_data, USB_BUFFER_SIZ,
                              callback_usb_async_complete, NULL,10);
    if(libusb_submit_transfer(xfr) < 0)
    {
        printf("Error\n");
        libusb_free_transfer(xfr);
    }
}

void db_write_usb_async(uint8_t payload_data[], struct accessory_t *accessory, db_usb_msg_t* usb_msg, uint16_t data_length,
                        uint8_t port) {
    usb_msg->port = port;
    uint16_t max_pack_size = get_db_usb_max_packet_size();
    if (data_length < max_pack_size) {
        usb_msg->pay_lenght = data_length;
        memcpy(usb_msg->payload, payload_data, data_length);

        struct libusb_transfer *xfr = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
                                  (data_length + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL,10);
        if(libusb_submit_transfer(xfr) < 0)
        {
            printf("Error\n");
            libusb_free_transfer(xfr);
        }
    } else { // split data
        uint16_t sent_data_length = 0;
        while(data_length < sent_data_length) {
            if ((sent_data_length - data_length) > max_pack_size) {
                usb_msg->pay_lenght = max_pack_size;
                memcpy(usb_msg->payload, &payload_data[sent_data_length], max_pack_size);
                struct libusb_transfer *xfr = libusb_alloc_transfer(0);

                libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
                                          (data_length + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL,10);
                if(libusb_submit_transfer(xfr) < 0)
                {
                    printf("Error\n");
                    libusb_free_transfer(xfr);
                }

                sent_data_length += usb_msg->pay_lenght; // (num_trans - DB_AOA_HEADER_LENGTH);
            } else {
                usb_msg->pay_lenght = data_length - sent_data_length;
                memcpy(usb_msg->payload, &payload_data[sent_data_length], max_pack_size);

                struct libusb_transfer *xfr = libusb_alloc_transfer(0);
                libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
                                          (data_length + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL,10);
                if(libusb_submit_transfer(xfr) < 0)
                {
                    printf("Error\n");
                    libusb_free_transfer(xfr);
                }
            }
        }
    }


    usb_msg->port = port;
    usb_msg->pay_lenght = data_length;

    struct libusb_transfer *xfr = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
            (data_length + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL,10);
    if(libusb_submit_transfer(xfr) < 0)
    {
        printf("Error\n");
        libusb_free_transfer(xfr);
    }
}

int main(int argc, char *argv[]) {
    db_usb_msg_t *usb_msg = db_usb_get_direct_buffer();

    int video_unix_socket = -1, proxy_sock = -1, status_sock = -1, communication_sock = -1;
    if (video_module_activated)
        video_unix_socket = open_configure_unix_socket();
    if (proxy_module_activated)
        proxy_sock = open_local_tcp_socket(APP_PORT_PROXY);
    if (status_module_activated)
        status_sock = open_local_tcp_socket(APP_PORT_STATUS);
    if (communication_module_activated)
        communication_sock = open_local_tcp_socket(APP_PORT_COMM);

    db_accessory_t accessory;
    init_db_accessory(&accessory); // blocking
    struct timeval tv;
    struct timeval tv_libusb_events;
    struct libusb_pollfd ** usb_fds;

    uint8_t tcp_buffer[TCP_BUFF_SIZ];
    fd_set readset; ssize_t tcp_received;
    tv_libusb_events.tv_sec = 0; tv_libusb_events.tv_usec = 0;
    usb_fds = (struct libusb_pollfd **) libusb_get_pollfds(NULL);


    while (keeprunning) {
        struct pollfd poll_fds[20];
        uint8_t count = 0;
        int usb_fd_count;
        for (usb_fd_count = 0; usb_fds[usb_fd_count]; usb_fd_count++) {
            poll_fds[usb_fd_count].fd = usb_fds[usb_fd_count]->fd;
            poll_fds[usb_fd_count].events = usb_fds[usb_fd_count]->events;
            count++;
        }
        if (video_module_activated) {
            poll_fds[count].fd = video_unix_socket;
            poll_fds[count].events = POLLIN;
            count++;
        }
        if (proxy_module_activated) {
            poll_fds[count].fd = proxy_sock;
            poll_fds[count].events = POLLIN;
            count++;
        }
        if (status_module_activated) {
            poll_fds[count].fd = status_sock;
            poll_fds[count].events = POLLIN;
            count++;
        }
        if (communication_module_activated) {
            poll_fds[count].fd = communication_sock;
            poll_fds[count].events = POLLIN;
            count++;
        }

        int ret = poll(poll_fds, count,10);
        if (ret == -1) {
            perror ("poll");
            return -1;
        } else if (ret == 0) {
            // TODO: handle timeout
            continue;
        } else {
            // check usb for new data
            int i = 0;
            for (; i < usb_fd_count; i++) {
                if (poll_fds[i].revents & POLLIN) {
                    libusb_handle_events_timeout(NULL, &tv_libusb_events);
                    db_read_usb_async(&accessory);
                }
            }
            // check sockets for new data
            for (; i < count; i++) {
                if (poll_fds[i].revents & POLLIN) {
                    if (video_module_activated && poll_fds[i].fd == video_unix_socket) {
                        tcp_received = recv(video_unix_socket, tcp_buffer, TCP_BUFF_SIZ, 0);
                        // TODO: send via USB
                    } else if (proxy_module_activated && poll_fds[i].fd == proxy_sock) {
                        tcp_received = recv(proxy_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                        // TODO: send via USB
                    } else if (status_module_activated && poll_fds[i].fd == status_sock) {
                        tcp_received = recv(status_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                        // TODO: send via USB
                    } else if (communication_module_activated && poll_fds[i].fd == communication_sock) {
                        tcp_received = recv(communication_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                        // TODO: send via USB
                    }
                }
            }
        }


/*        tv.tv_sec = 0; tv.tv_usec = 10;

        FD_ZERO(&readset);
        if (video_module_activated) {
            max_sd = video_unix_socket;
            FD_SET(video_unix_socket, &readset);
        }
        if (proxy_module_activated) {
            FD_SET(proxy_sock, &readset);
            if (proxy_sock > max_sd)
                max_sd = proxy_sock;
        }
        if (status_module_activated) {
            FD_SET(status_sock, &readset);
            if (status_sock > max_sd)
                max_sd = status_sock;
        }
        if (communication_module_activated) {
            FD_SET(communication_sock, &readset);
            if (communication_sock > max_sd)
                max_sd = communication_sock;
        }

        for (int i = 0; usb_fds[i]; i++) {
            if (usb_fds[i]->events == POLLIN) { // only use read descriptors
                FD_SET(usb_fds[i]->fd, &readset);
                if (usb_fds[i]->fd > max_sd)
                    max_sd = usb_fds[i]->fd;
            }
        }

        int select_return = select(max_sd + 1, &readset, NULL, NULL, &tv); // only triggers once!
        if (select_return == -1) {
            perror("DB_VIDEO_GND: select() returned error: ");
        } else if (select_return > 0) {
            libusb_handle_events_timeout(NULL, &tv_libusb_events);

            if (video_module_activated && FD_ISSET(video_unix_socket, &readset)) {
                tcp_received = recv(video_unix_socket, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_VIDEO);
            }
            if (proxy_module_activated && FD_ISSET(proxy_sock, &readset)) {
                tcp_received = recv(proxy_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_PROXY);
            }
            if (status_module_activated && FD_ISSET(status_sock, &readset)) {
                tcp_received = recv(status_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_STATUS);
            }
            if (communication_module_activated && FD_ISSET(communication_sock, &readset)) {
                tcp_received = recv(communication_sock, tcp_buffer, TCP_BUFF_SIZ, 0);
                db_usb_send(&accessory, tcp_buffer, tcp_received, DB_PORT_COMM);
            }
            for (int i = 0; usb_fds[i] != NULL; i++) {
                if (FD_ISSET(usb_fds[i]->fd, &readset)) {
                    libusb_handle_events_timeout(NULL, &tv_libusb_events);
                    unsigned char *data;
                    data = malloc(512);
                    struct libusb_transfer *xfr = libusb_alloc_transfer(0);
                    libusb_fill_bulk_transfer(xfr, accessory.handle, AOA_ACCESSORY_EP_IN, data,512,
                                              callbackUSBTransferComplete, NULL,1);
                    if(libusb_submit_transfer(xfr) < 0)
                    {
                        printf("Error\n");
                        libusb_free_transfer(xfr);
                        free(data);
                    }
                    free(data);
                }
            }
        } else {
            libusb_handle_events_timeout(NULL, &tv_libusb_events);
            // db_usb_receive_debug(&accessory);
            // TODO send ping to phone to get it out of the read loop to be able to write stuff
        }*/
    }

    // clean up and exit
    libusb_free_pollfds((const struct libusb_pollfd **) usb_fds);
    exit_close_aoa_device(&accessory);
    close(video_unix_socket);
    close(proxy_sock);
    close(status_sock);
    close(communication_sock);
    unlink(DB_UNIX_DOMAIN_VIDEO_PATH);
    return 0;
}