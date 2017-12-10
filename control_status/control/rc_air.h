//
// Created by Wolfgang Christl on 10.12.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#ifndef CONTROL_STATUS_RC_AIR_H
#define CONTROL_STATUS_RC_AIR_H

uint8_t serial_data_buffer[2048] = {0}; // write the data for the serial port in here!

int conf_rc_serial_protocol_air(int new_rc_protocol);
void generate_rc_serial_message(uint8_t *db_rc_protocol);

#endif //CONTROL_STATUS_RC_AIR_H
