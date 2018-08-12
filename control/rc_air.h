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

#ifndef CONTROL_STATUS_RC_AIR_H
#define CONTROL_STATUS_RC_AIR_H

#define RC_SERIAL_PROT_MSPV1            1
#define RC_SERIAL_PROT_MSPV2            2
#define RC_SERIAL_PROT_MAVLINKV1        3
#define RC_SERIAL_PROT_MAVLINKV2        4
#define RC_SERIAL_PROT_SUMD             5

extern uint8_t serial_data_buffer[1024]; // write the rc protocol data for the serial port in here! init in rc_air

void conf_rc_serial_protocol_air(int new_rc_protocol, char use_sumd);
int generate_rc_serial_message(uint8_t *db_rc_protocol);
void open_rc_rx_shm();

#endif //CONTROL_STATUS_RC_AIR_H
