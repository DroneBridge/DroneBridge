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
#include <sys/socket.h>
#include <sys/un.h>
#include <zconf.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <errno.h>
#include <signal.h>
#include "linux_aoa.h"
#include "../common/db_protocol.h"
#include "../common/ccolors.h"
#include "db_usb.h"


#define TCP_BUFF_SIZ    4096
#define USB_BUFFER_SIZ  1024
#define MAX_WRITE_TIMEOUT   200  // max time [ms] that the USBBridge is allowed to not send data to the GCS. Send wake to stop blocking android accessory api

bool keeprunning = true;
bool video_module_activated = false;
bool communication_module_activated = false;
bool proxy_module_activated = false;
bool status_module_activated = false;
int video_unix_socket = -1, proxy_sock = -1, status_sock = -1, communication_sock = -1;

uint8_t usb_in_data[USB_BUFFER_SIZ];
int usb_parser_state = DB_USB_PARSER_SEARCHING_HEADER;
uint16_t db_usb_parser_payload_size = 0;
uint16_t usb_parser_buff_size = 0;
uint8_t *db_usb_parser_buffer;
uint8_t db_usb_parser_port = 0;

struct timeval timecheck;

struct libusb_pollfd ** usb_fds;
bool device_connected = false;

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
    printf("DB_USB: Openened TCP socket\n");
    return sockfd;
}

long get_time() {
    gettimeofday(&timecheck, NULL);
    return (long) timecheck.tv_sec * 1000 + (long) timecheck.tv_usec / 1000;
}

void usb_fd_added(int fd, short events, void *user_data){
    printf("DB_USB: Adding new file descriptor to poll\n");
    int i = 0;
    for(; usb_fds[i]; i++) {}
    usb_fds[i]->fd = fd;
    usb_fds[i]->events = events;
}

void usb_fd_removed(int fd, void *user_data) {
    printf("DB_USB: Removing file descriptor from poll\n");
    for(int i = 0; usb_fds[i]; i++) {
        if (usb_fds[i]->fd == fd) {
            usb_fds[i] = NULL;
            break;
        }
    }
}

void db_usb_route_data_tcp(uint8_t payload[], uint8_t port, uint16_t payload_size) {
    printf("Got some data (%i) form GCS: %s\n", payload_size, payload);
    switch (port) {
        case DB_PORT_VIDEO:
            fprintf(stderr, "DB_USB: Error video module does not accept incoming data!\n");
            break;
        case DB_PORT_PROXY:
            if (proxy_module_activated) {
                if (send(proxy_sock, payload, payload_size, 0) < 0)
                    perror("DB_USB: Error sending to proxy module");
            }
            break;
        case DB_PORT_STATUS:
            if (status_module_activated) {
                if (send(status_sock, payload, payload_size, 0) < 0)
                    perror("DB_USB: Error sending to status module");
            }
            break;
        case DB_PORT_COMM:
            if (communication_module_activated) {
                if (send(communication_sock, payload, payload_size, 0) < 0)
                    perror("DB_USB: Error sending to communication module");
            }
            break;
        case DB_USB_PORT_TIMEOUT_WAKE:
            break;
        default:
            fprintf(stderr, "DB_USB: Unknown destination port. Use DB RAW protocol ports!\n");
            break;
    }
}

/**
 * Incoming data from GCS. Forward to sockets if complete
 * @param buffer USB buffer
 * @param length length of USB buffer
 */
void process_db_usb_proto(unsigned char buffer[], int length) {
    if (usb_parser_state == DB_USB_PARSER_SEARCHING_HEADER && length >= DB_AOA_HEADER_LENGTH) {
        // check for header
        if (buffer[0] == 'D' && buffer[1] == 'B' && buffer[2] == DB_USB_PROTO_VERSION) {
            db_usb_parser_port = buffer[3];
            db_usb_parser_payload_size = buffer[4] | (buffer[5] << 8);
            if (db_usb_parser_payload_size > DATA_UNI_LENGTH) {
                fprintf(stderr, "DB_USB: Specified payload too big for raw protocol (%i > %i). Ignoring\n",
                        db_usb_parser_payload_size, DATA_UNI_LENGTH);
                return;
            }
            if ((length - DB_AOA_HEADER_LENGTH) == db_usb_parser_payload_size) {
                // payload is not split in between packets -> ready for processing. zero copy operation
                db_usb_route_data_tcp(&buffer[DB_AOA_HEADER_LENGTH], db_usb_parser_port, db_usb_parser_payload_size);
            } else {
                // payload data incomplete -> wait for next packet
                usb_parser_buff_size += (length - DB_AOA_HEADER_LENGTH);
                db_usb_parser_buffer = (uint8_t *) malloc(db_usb_parser_payload_size * sizeof(uint8_t));
                memcpy(db_usb_parser_buffer, &buffer[DB_AOA_HEADER_LENGTH], (length - DB_AOA_HEADER_LENGTH));
                usb_parser_state = DB_USB_PARSER_AWAITING_PAYLOAD;
            }
        }
    } else if (usb_parser_state == DB_USB_PARSER_AWAITING_PAYLOAD) {
//        printf("%i %i %i\n", usb_parser_buff_size, length, db_usb_parser_payload_size);
        // check if packet completes payload
        if ((usb_parser_buff_size + length) == db_usb_parser_payload_size) {
            // Copy and process buffer
            memcpy(&db_usb_parser_buffer[usb_parser_buff_size], buffer, length);
            usb_parser_buff_size += length;
            db_usb_route_data_tcp(db_usb_parser_buffer, db_usb_parser_port, db_usb_parser_payload_size);
            free(db_usb_parser_buffer);
            usb_parser_buff_size = 0;
            usb_parser_state = DB_USB_PARSER_SEARCHING_HEADER;
        } else if ((usb_parser_buff_size + length) < db_usb_parser_payload_size) {
            // append data to buffer. Still waiting for new data to complete payload
            memcpy(&db_usb_parser_buffer[usb_parser_buff_size], buffer, length);
            usb_parser_buff_size += length;
        } else if ((usb_parser_buff_size + length) > db_usb_parser_payload_size) {
            free(db_usb_parser_buffer);
            usb_parser_buff_size = 0;
            usb_parser_state = DB_USB_PARSER_SEARCHING_HEADER;
            fprintf(stderr, "DB_USB: DB USB protocol does not allow packets containing payload of two msgs!\n");
        }
    }
}

void callback_usb_async_complete(struct libusb_transfer *xfr) {
    switch(xfr->status) {
        case LIBUSB_TRANSFER_COMPLETED:
            switch (xfr->endpoint) {
                case AOA_ACCESSORY_EP_IN:
                    printf("DB_USB: Received %i\n", xfr->actual_length);
                    process_db_usb_proto(xfr->buffer, xfr->actual_length);
                    break;
                case AOA_ACCESSORY_EP_OUT:
                    // printf("DB_USB: Transferred %i\n", xfr->actual_length);
                    break;
            }
            // xfr->buffer
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            printf("DB_USB: Transfer cancelled\n");
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            printf("DB_USB: No device!\n");
            device_connected = false;
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
//            printf("DB_USB: Timed out!\n");
            break;
        case LIBUSB_TRANSFER_ERROR:
            printf("DB_USB: Transfer error!\n");
            break;
        case LIBUSB_TRANSFER_STALL:
            printf("DB_USB: Transfer stall!\n");
            break;
        case LIBUSB_TRANSFER_OVERFLOW:
            printf("DB_USB: Transfer overflow!\n");
            // Various type of errors here
            break;
    }
    libusb_free_transfer(xfr);
}

/**
 * Used to unblock the android accessory stream read API. Otherwise device might block forever and not be able to send
 * @param accessory
 * @param usb_msg
 */
void send_timeout_wake(struct accessory_t *accessory, db_usb_msg_t* usb_msg, long *last_write) {
    if (!device_connected)
        return;
//    printf("DB_USB: Sending timeout wake\n");
    usb_msg->pay_lenght = 1;
    usb_msg->port = DB_USB_PORT_TIMEOUT_WAKE;
    usb_msg->payload[0] = 0;
    struct libusb_transfer *xfr = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, (unsigned char *) usb_msg,
                              (1 + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL, 1000);
    if(libusb_submit_transfer(xfr) < 0)
    {
        perror("DB_USB: Error submitting timeout wake transfer");
        device_connected = false;
        libusb_free_transfer(xfr);
    }
    *last_write = get_time();
}

/**
 * Init async read
 *
 * @param accessory
 */
void db_read_usb_async(struct accessory_t *accessory) {
    struct libusb_transfer *xfr = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_IN, usb_in_data, USB_BUFFER_SIZ,
                              callback_usb_async_complete, NULL,100);
    if(libusb_submit_transfer(xfr) < 0)
    {
        perror("DB_USB: Error submitting transfer");
        device_connected = false;
        libusb_free_transfer(xfr);
    }
}

/**
 * Zero copy transmission
 *
 * @param accessory     Inited android accessory_t
 * @param usb_msg       Points to raw_usb_msg_buff
 * @param data_length   Length of payload
 * @param port          Destination port of USB message. Use DB RAW protocol ports
 */
void db_usb_write_async_zc(struct accessory_t *accessory, db_usb_msg_t* usb_msg, uint16_t data_length, uint8_t port) {
    usb_msg->port = port;
    uint16_t max_pack_size = get_db_usb_max_packet_size();
    if (data_length <= max_pack_size) {  // no splitting required
        usb_msg->pay_lenght = data_length;
        struct libusb_transfer *xfr = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, (unsigned char *) usb_msg,
                                  (data_length + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL, 1000);
        if(libusb_submit_transfer(xfr) < 0)
        {
            perror("DB_USB: Error submitting transfer");
            device_connected = false;
            libusb_free_transfer(xfr);
        }
    } else {  // split into multiple transmissions. Only send header once
        uint16_t sent_data_length = 0;
        usb_msg->pay_lenght = data_length;
        while(sent_data_length < data_length) {
            struct libusb_transfer *xfr = libusb_alloc_transfer(0);
            libusb_fill_bulk_transfer(xfr, accessory->handle, AOA_ACCESSORY_EP_OUT, &raw_usb_msg_buff[sent_data_length],
                    (usb_msg->pay_lenght + DB_AOA_HEADER_LENGTH), callback_usb_async_complete, NULL,1000);
            if(libusb_submit_transfer(xfr) < 0)
            {
                perror("DB_USB: Error submitting transfer");
                device_connected = false;
                libusb_free_transfer(xfr);
                return;
            }

            sent_data_length += usb_msg->pay_lenght; // (num_trans - DB_AOA_HEADER_LENGTH);
        }
    }
}

int main(int argc, char *argv[]) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = intHandler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    process_command_line_args(argc, argv);

    db_usb_msg_t *usb_msg = db_usb_get_direct_buffer();
    usb_msg->ident[0] = 'D';
    usb_msg->ident[1] = 'B';
    usb_msg->ident[2] = DB_USB_PROTO_VERSION;
    long last_write = 0;

    db_accessory_t accessory;
    if (init_db_accessory(&accessory) == -1) { // blocking
        keeprunning = false;
        device_connected = false;
    } else
        device_connected = true;

    if (video_module_activated)
        video_unix_socket = open_configure_unix_socket();
    if (proxy_module_activated)
        proxy_sock = open_local_tcp_socket(APP_PORT_PROXY);
    if (status_module_activated)
        status_sock = open_local_tcp_socket(APP_PORT_STATUS);
    if (communication_module_activated)
        communication_sock = open_local_tcp_socket(APP_PORT_COMM);

    struct timeval tv_libusb_events;

    ssize_t tcp_num_recv;
    tv_libusb_events.tv_sec = 0; tv_libusb_events.tv_usec = 0;
    usb_fds = (struct libusb_pollfd **) libusb_get_pollfds(NULL);
    libusb_set_pollfd_notifiers(NULL, usb_fd_added, usb_fd_removed, NULL);

    while (keeprunning) {
        if (!device_connected) {
            //libusb_free_pollfds((const struct libusb_pollfd **) usb_fds);
            //libusb_set_pollfd_notifiers(NULL, NULL, NULL, NULL);
            exit_close_aoa_device(&accessory);

            usb_msg = db_usb_get_direct_buffer();
            usb_msg->ident[0] = 'D';
            usb_msg->ident[1] = 'B';
            usb_msg->ident[2] = DB_USB_PROTO_VERSION;
            last_write = 0;
            init_db_accessory(&accessory); // blocking
            usb_parser_state = DB_USB_PARSER_SEARCHING_HEADER;

            device_connected = true;
        }

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

        int ret = poll(poll_fds, count, MAX_WRITE_TIMEOUT);
        if (ret == -1) {
            perror ("poll");
            keeprunning = false;
        } else if (ret == 0) {
            send_timeout_wake(&accessory, usb_msg, &last_write);
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
                        tcp_num_recv = recv(video_unix_socket, usb_msg->payload, DB_AOA_MAX_PAY_LENGTH, 0);
                        db_usb_write_async_zc(&accessory, usb_msg, tcp_num_recv, DB_PORT_VIDEO);
                        last_write = get_time();
                    } else if (proxy_module_activated && poll_fds[i].fd == proxy_sock) {
                        tcp_num_recv = recv(proxy_sock, usb_msg->payload, DB_AOA_MAX_PAY_LENGTH, 0);
                        db_usb_write_async_zc(&accessory, usb_msg, tcp_num_recv, DB_PORT_PROXY);
                        last_write = get_time();
                    } else if (status_module_activated && poll_fds[i].fd == status_sock) {
                        tcp_num_recv = recv(status_sock, usb_msg->payload, DB_AOA_MAX_PAY_LENGTH, 0);
                        db_usb_write_async_zc(&accessory, usb_msg, tcp_num_recv, DB_PORT_STATUS);
                        last_write = get_time();
                    } else if (communication_module_activated && poll_fds[i].fd == communication_sock) {
                        tcp_num_recv = recv(communication_sock, usb_msg->payload, DB_AOA_MAX_PAY_LENGTH, 0);
                        db_usb_write_async_zc(&accessory, usb_msg, tcp_num_recv, DB_PORT_COMM);
                        last_write = get_time();
                    }
                }
            }
            if ((get_time() - last_write) >= MAX_WRITE_TIMEOUT) {
                send_timeout_wake(&accessory, usb_msg, &last_write);
            }
        }
    }

    // clean up and exit
    printf("DB_USB: Closing sockets\n");
    libusb_free_pollfds((const struct libusb_pollfd **) usb_fds);
    shutdown(video_unix_socket, SHUT_RDWR);
    shutdown(proxy_sock, SHUT_RDWR);
    shutdown(status_sock, SHUT_RDWR);
    shutdown(communication_sock, SHUT_RDWR);
    close(video_unix_socket);
    close(proxy_sock);
    close(status_sock);
    close(communication_sock);
    libusb_set_pollfd_notifiers(NULL, NULL, NULL, NULL);
    exit_close_aoa_device(&accessory);
    unlink(DB_UNIX_DOMAIN_VIDEO_PATH);
    printf("DB_USB: Terminated\n");
    return 0;
}