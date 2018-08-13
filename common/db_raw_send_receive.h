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

#ifndef CONTROL_DB_RAW_SEND_H
#define CONTROL_DB_RAW_SEND_H

#include "db_protocol.h"
#include <stdint.h>
#include <linux/if_packet.h>

// That is the buffer that will be sent over the socket. Create a pointer to a part of this array and fill it with your
// data, like e.g.:
// struct uav_rc_status_update_message *rc_status_update_data = (struct uav_rc_status_update_message *) (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
extern uint8_t monitor_framebuffer[RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DATA_UNI_LENGTH];

typedef struct {
    int db_socket;
    struct sockaddr_ll db_socket_addr;
} db_socket;

db_socket open_db_socket(char *ifName, uint8_t comm_id, char trans_mode, int bitrate_option,
                                   uint8_t send_direction, uint8_t receive_new_port);
int open_socket_send_receive(char *ifName, uint8_t comm_id, char trans_mode, int bitrate_option, uint8_t direction, 
                             uint8_t new_port);
uint8_t update_seq_num(uint8_t *old_seq_num);
int send_packet(uint8_t payload[], uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num);
int send_packet_div(db_socket *a_db_socket, uint8_t payload[], uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num);
int send_packet_hp(uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num);
int send_packet_hp_div(db_socket *a_db_socket, uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num);
void close_socket_send_receive();

#endif //CONTROL_DB_RAW_SEND_H