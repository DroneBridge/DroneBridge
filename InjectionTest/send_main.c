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

#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <zconf.h>
#include "../common/db_raw_send_receive.h"

// DroneBridge options
#define INTERFACE "wlx8416f916382c"
#define FRAME_TYPE  2   // 1=RTS; 2=DATA
#define DATARATE    11  // Mbit
#define COMMID      16
#define DST_PORT    10  // destination port
#define RECV_PORT   DST_PORT

#define PAYLOAD_BUFF_SIZ 1024

int keep_running = 1;

void int_handler(int dummy) {
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    char interface[IFNAMSIZ];
    strcpy(interface, INTERFACE);

    db_socket_t raw_socket = open_db_socket(interface, COMMID, 'm', DATARATE, DB_DIREC_GROUND,
                                            RECV_PORT, (uint8_t) FRAME_TYPE);

    uint8_t seq_num = 0;
    uint8_t payload[PAYLOAD_BUFF_SIZ];
    memset(payload, 1, PAYLOAD_BUFF_SIZ);

    while (keep_running) {
        db_send_div(&raw_socket, payload, DST_PORT, PAYLOAD_BUFF_SIZ, update_seq_num(&seq_num), 0);
        printf(".");
        fflush(stdout);
        usleep((unsigned int) 1e6);
    }
    close(raw_socket.db_socket);
    printf("Terminated");
}