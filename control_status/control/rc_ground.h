//
// Created by cyber on 02.12.17.
//

#ifndef CONTROL_TX_H
#define CONTROL_TX_H

int send_rc_packet(uint16_t channel_data[]);
int conf_rc_protocol(int new_rc_protocol);
void open_rc_tx_shm();

#endif //CONTROL_TX_H
