/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2017 Wolfgang Christl
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
#include <sys/socket.h>
#include "db_common.h"


/**
 * Clear/Drain receive buffer of socket. Receive and clear all bytes in the queue. Used after applying BPF filter. In period
 * between opening socket and applying filter the socket might received packets that do not match the filter. Clear all
 * data to only output data the user requested via the filter.
 *
 * Takes min. 100 usec
 *
 * @param socket_fd The socket file descriptor of an db_socket in raw mode
 */
void clear_socket_buffer(int socket_fd) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    // Set timeout on (blocking socket otherwise will not return)
    int recv_buff_size = 2048;
    uint8_t recv_buff[recv_buff_size];
    while(recv(socket_fd, recv_buff, recv_buff_size, 0) > 0) {}
    // Reset timeout
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
}


void print_buffer(uint8_t buffer[], int num_bytes){
    for (int i = 0; i < num_bytes; ++i) {
        LOG_SYS_STD(LOG_INFO, "%2x ", buffer[i]);
    }
    LOG_SYS_STD(LOG_INFO, "\n");
}

/**
 * Reads the current low-voltage value from RPi
 * @return 1 if currently not enough voltage supplied to Pi; 0 if all OK
 */
uint8_t get_undervolt(void){
    uint8_t uvolt = 10;
    FILE *fp;
    char path[1035];
    fp = popen("vcgencmd get_throttled", "r");
    if (fp == NULL) {
        LOG_SYS_STD(LOG_WARNING, "Failed to get raspberry pi under-voltage notice\n" );
        exit(1);
    }
    if (fgets(path, sizeof(path)-1, fp) != NULL) {
        char *dummy;
        uint32_t iuvolt = (uint32_t) strtoul(&path[10], &dummy, 16);
        uvolt = (uint8_t) ((uint8_t) iuvolt & 1);  // get under volt bit - current voltage status
    }
    pclose(fp);
    return uvolt;
}
