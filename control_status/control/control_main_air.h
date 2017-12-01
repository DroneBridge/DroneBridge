//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#ifndef CONTROL_RX_MAIN_H
#define CONTROL_RX_MAIN_H

#include <stdint.h>
#include <net/if.h>

int open_socket_sending(char ifName[IFNAMSIZ], unsigned char dest_mac[6], char trans_mode, int bitrate_option,
                        int frame_type, uint8_t direction);
int send_packet(const int8_t payload[], const uint8_t dest_port);
void close_socket_ground_comm();

#endif //CONTROL_RX_MAIN_H
