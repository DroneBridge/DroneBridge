//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#ifndef CONTROL_DB_RAW_SEND_H
#define CONTROL_DB_RAW_SEND_H

#include "db_protocol.h"
#include <stdint.h>
#include <net/if.h>

// That is the buffer that will be sent over the socket. Create a pointer to a part of this array and fill it with your
// data, like e.g.:
// struct data_rc_status_update *rc_status_update_data = (struct data_rc_status_update *) (monitor_framebuffer_uni + RADIOTAP_LENGTH + DB80211_HEADER_LENGTH);
uint8_t monitor_framebuffer_uni[RADIOTAP_LENGTH + DB80211_HEADER_LENGTH + DATA_UNI_LENGTH];


int open_socket_sending(char ifName[IFNAMSIZ], uint8_t comm_id[4], char trans_mode, int bitrate_option, int frame_type,
                        uint8_t direction);
int send_packet(int8_t payload[], uint8_t dest_port, u_int16_t payload_length);
int send_packet_hp(uint8_t dest_port, u_int16_t payload_length);
void close_socket_send();

#endif //CONTROL_DB_RAW_SEND_H