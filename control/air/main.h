//
// Created by cyber on 26.11.17.
//

#ifndef CONTROL_RX_MAIN_H
#define CONTROL_RX_MAIN_H

#include <stdint.h>
#include <net/if.h>

int open_socket_sending(char ifName[IFNAMSIZ], unsigned char dest_mac[6], char trans_mode, int bitrate_option,
                        int frame_type, uint8_t direction);
int send_packet(const uint8_t payload[], const uint8_t dest_port);

#endif //CONTROL_RX_MAIN_H
