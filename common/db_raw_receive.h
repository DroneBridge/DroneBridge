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

#ifndef STATUS_DB_RECEIVE_H
#define STATUS_DB_RECEIVE_H

#include <stdint.h>
#include <net/if.h>

int setBPF(int newsocket, uint8_t new_comm_id, uint8_t direction, uint8_t port);
int bindsocket(int newsocket, char the_mode, char new_ifname[IFNAMSIZ]);
int set_socket_nonblocking(int the_socket);
int set_socket_timeout(int the_socketfd, int time_out_s ,int time_out_us);
uint16_t get_db_payload(uint8_t *receive_buffer, ssize_t receive_length, uint8_t *payload_buffer, uint8_t *seq_num,
        uint16_t *radiotap_length);
uint8_t count_lost_packets(uint8_t last_seq_num, uint8_t received_seq_num);

#endif //STATUS_DB_RECEIVE_H
