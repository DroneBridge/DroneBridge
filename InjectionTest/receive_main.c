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

/**
 * Example on how to use DroneBridge common library to receive data.
 * This example receives on the port of the status module.
 * Set port & adapter name to match your configuration
 */

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <zconf.h>
#include "../common/db_raw_send_receive.h"
#include "../common/db_raw_receive.h"

#define BUFFER_SIZE 2048

#define ADAPTER_NAME "wlx8416f916382c"    // Name of WiFi adapter. Must be in monitor mode
#define DB_RECEIVING_PORT DB_PORT_STATUS  // Receive data on DB raw port for status module
#define DB_SEND_DIRECTION DB_DIREC_DRONE  // Send data in direction of UAV

int keep_going = 1;

void sig_handler(int sig) {
    printf("DroneBridge example receiver: Terminating...\n");
    keep_going = 0;
}

int main(int argc, char *argv[]) {
    uint8_t buffer[BUFFER_SIZE];
    uint8_t payload_buff[DATA_UNI_LENGTH];
    uint8_t seq_num = 0;
    memset(buffer, 0, BUFFER_SIZE);

    uint8_t comm_id = 200;
    char db_mode = 'm';
    char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
    strncpy(adapters[0], ADAPTER_NAME, IFNAMSIZ);

    // init DroneBridge sockets for raw protocol
    db_socket_t raw_interfaces[DB_MAX_ADAPTERS];  // array of DroneBridge sockets
    memset(raw_interfaces, 0, DB_MAX_ADAPTERS);
    raw_interfaces[0] = open_db_socket(adapters[0], comm_id, db_mode, 6, DB_SEND_DIRECTION,
                                       DB_RECEIVING_PORT, DB_FRAMETYPE_DEFAULT);
    // Some stuff for proper termination
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sig_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    printf("DroneBridge example receiver: Waiting for data\n");
    while (keep_going) {
        uint16_t radiotap_length = 0;
        ssize_t received_bytes = recv(raw_interfaces[0].db_socket, buffer, BUFFER_SIZE, 0);
        uint16_t payload_length = get_db_payload(buffer, received_bytes, payload_buff, &seq_num, &radiotap_length);
        int8_t rssi = get_rssi(buffer, radiotap_length);
        printf("Received raw frame with %zi bytes & %i bytes of payload (%i dBm)\n", received_bytes,
               payload_length, rssi);
    }
    for (int i = 0; i < DB_MAX_ADAPTERS; i++)
        close(raw_interfaces[i].db_socket);
    return 0;
}
