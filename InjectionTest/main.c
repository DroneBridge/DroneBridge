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

#include "main.h"
#include "../common/db_raw_send_receive.h"

int main(int argc, char *argv[]) {
    char interface[IFNAMSIZ];
    strcpy(interface, argv[1]);

    db_socket_t socket = open_db_socket(interface, 16, 'm', 11, DB_DIREC_GROUND, 10, (uint8_t) atoi(argv[2]));

    uint8_t payload[payload_size];
    memset(payload, 1, payload_size);

    for (int i = 0; i < 100; ++i) {
        send_packet_div(&socket, payload, 10, payload_size, 66, 1);
        printf(".");
        fflush(stdout);
        usleep((unsigned int) 1e6);
    }
}