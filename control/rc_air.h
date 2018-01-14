//
// Created by Wolfgang Christl on 10.12.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#ifndef CONTROL_STATUS_RC_AIR_H
#define CONTROL_STATUS_RC_AIR_H

extern uint8_t serial_data_buffer[1024]; // write the rc protocol data for the serial port in here! init in rc_air

int conf_rc_serial_protocol_air(int new_rc_protocol);
int generate_rc_serial_message(uint8_t *db_rc_protocol);

#endif //CONTROL_STATUS_RC_AIR_H
