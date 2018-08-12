/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2018 Wolfgang Christl
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

#include <stdint.h>
#include <stdio.h>
#include "rc_air.h"
#include "../common/db_protocol.h"
#include "../common/db_crc.h"
#include "../common/ccolors.h"
#include "../common/mavlink/c_library_v2/common/mavlink.h"
#include "../common/shared_memory.h"


int serial_rc_protocol, i_rc_air, i_sumd_air, i_rc;
db_rc_values *shm_rc_values = NULL;
uint8_t serial_data_buffer[1024] = {0}; // write the data for the serial port in here!

uint16_t rc_channels[DB_RC_NUM_CHANNELS] = {0}, sumd_multiplier = 8, crc_sumd_air = 0;;
uint8_t crc_mspv1_air = 0, crc_mspv2_air = 0, crc_db_rc = 0;

void generate_sumd(uint16_t *rc_channel_data){
    for (int i = 0; i < DB_RC_NUM_CHANNELS; ++i) rc_channel_data[i] = rc_channel_data[i] * sumd_multiplier;
    serial_data_buffer[0] = 0xa8;
    serial_data_buffer[1] = 0x01;
    serial_data_buffer[2] = 0x0c;   // 12 channels
    // A
    serial_data_buffer[3] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    serial_data_buffer[4] = (uint8_t) (rc_channel_data[0] & 0xFF);
    // E
    serial_data_buffer[5] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    serial_data_buffer[6] = (uint8_t) (rc_channel_data[1] & 0xFF);
    // T
    serial_data_buffer[7] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    serial_data_buffer[8] = (uint8_t) (rc_channel_data[2] & 0xFF);
    // R
    serial_data_buffer[9] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    serial_data_buffer[10] = (uint8_t) (rc_channel_data[3] & 0xFF);
    // 5
    serial_data_buffer[11] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    serial_data_buffer[12] = (uint8_t) (rc_channel_data[4] & 0xFF);
    // 6
    serial_data_buffer[13] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    serial_data_buffer[14] = (uint8_t) (rc_channel_data[5] & 0xFF);
    // 7
    serial_data_buffer[15] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    serial_data_buffer[16] = (uint8_t) (rc_channel_data[6] & 0xFF);
    // 8
    serial_data_buffer[17] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    serial_data_buffer[18] = (uint8_t) (rc_channel_data[7] & 0xFF);
    // 9
    serial_data_buffer[19] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    serial_data_buffer[20] = (uint8_t) (rc_channel_data[8] & 0xFF);
    // 10
    serial_data_buffer[21] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    serial_data_buffer[22] = (uint8_t) (rc_channel_data[9] & 0xFF);
    // 11
    serial_data_buffer[23] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    serial_data_buffer[24] = (uint8_t) (rc_channel_data[10] & 0xFF);
    // 12
    serial_data_buffer[25] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    serial_data_buffer[26] = (uint8_t) (rc_channel_data[11] & 0xFF);
    // crc
    for (i_sumd_air = 0; i_sumd_air < 27; i_sumd_air++) {
        crc_sumd_air = (crc_sumd_air << 8) ^ crc_sumd_table[(crc_sumd_air >> 8) ^ serial_data_buffer[i_sumd_air]];
    }
    serial_data_buffer[27] = (uint8_t) ((crc_sumd_air >> 8) & 0xFF);
    serial_data_buffer[28] = (uint8_t) (crc_sumd_air & 0xFF);
    crc_sumd_air = 0;
}

void generate_msp(uint16_t *rc_channel_data) {
    serial_data_buffer[0] = 0x24;
    serial_data_buffer[1] = 0x4d;
    serial_data_buffer[2] = 0x3c;
    serial_data_buffer[3] = 0x18;   // payload size
    serial_data_buffer[4] = 0xc8;
    // A
    serial_data_buffer[8] = (uint8_t) (rc_channel_data[0] & 0xFF);
    serial_data_buffer[9] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    // E
    serial_data_buffer[10] = (uint8_t) (rc_channel_data[1] & 0xFF);
    serial_data_buffer[11] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    // T
    serial_data_buffer[12] = (uint8_t) (rc_channel_data[2] & 0xFF);
    serial_data_buffer[13] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    // R
    serial_data_buffer[14] = (uint8_t) (rc_channel_data[3] & 0xFF);
    serial_data_buffer[15] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    // 5
    serial_data_buffer[16] = (uint8_t) (rc_channel_data[4] & 0xFF);
    serial_data_buffer[17] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    // 6
    serial_data_buffer[18] = (uint8_t) (rc_channel_data[5] & 0xFF);
    serial_data_buffer[19] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    // 7
    serial_data_buffer[20] = (uint8_t) (rc_channel_data[6] & 0xFF);
    serial_data_buffer[21] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    // 8
    serial_data_buffer[22] = (uint8_t) (rc_channel_data[7] & 0xFF);
    serial_data_buffer[23] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    // 9
    serial_data_buffer[24] = (uint8_t) (rc_channel_data[8] & 0xFF);
    serial_data_buffer[25] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    // 10
    serial_data_buffer[26] = (uint8_t) (rc_channel_data[9] & 0xFF);
    serial_data_buffer[27] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    // 11
    serial_data_buffer[28] = (uint8_t) (rc_channel_data[10] & 0xFF);
    serial_data_buffer[29] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    // 12
    serial_data_buffer[30] = (uint8_t) (rc_channel_data[11] & 0xFF);
    serial_data_buffer[31] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    // CRC
    crc_mspv1_air = 0; // I think it is just a simple xor of bytes so no real crc really but a simple check sum
    for (int i = 3; i < 29; i++) {
        crc_mspv1_air ^= (serial_data_buffer[i] & 0xFF);
    }
    serial_data_buffer[29] = crc_mspv1_air;
}

void generate_mspv2(uint16_t *rc_channel_data) {
    serial_data_buffer[0] = 0x24;
    serial_data_buffer[1] = 0x58;
    serial_data_buffer[2] = 0x3c;
    serial_data_buffer[3] = 0x00; // flag
    serial_data_buffer[4] = 0xc8; // function
    serial_data_buffer[5] = 0x00; // function
    serial_data_buffer[6] = 0x18; // payload size
    serial_data_buffer[7] = 0x00; // payload size
    // A
    serial_data_buffer[8] = (uint8_t) (rc_channel_data[0] & 0xFF);
    serial_data_buffer[9] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    // E
    serial_data_buffer[10] = (uint8_t) (rc_channel_data[1] & 0xFF);
    serial_data_buffer[11] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    // T
    serial_data_buffer[12] = (uint8_t) (rc_channel_data[2] & 0xFF);
    serial_data_buffer[13] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    // R
    serial_data_buffer[14] = (uint8_t) (rc_channel_data[3] & 0xFF);
    serial_data_buffer[15] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    // 5
    serial_data_buffer[16] = (uint8_t) (rc_channel_data[4] & 0xFF);
    serial_data_buffer[17] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    // 6
    serial_data_buffer[18] = (uint8_t) (rc_channel_data[5] & 0xFF);
    serial_data_buffer[19] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    // 7
    serial_data_buffer[20] = (uint8_t) (rc_channel_data[6] & 0xFF);
    serial_data_buffer[21] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    // 8
    serial_data_buffer[22] = (uint8_t) (rc_channel_data[7] & 0xFF);
    serial_data_buffer[23] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    // 9
    serial_data_buffer[24] = (uint8_t) (rc_channel_data[8] & 0xFF);
    serial_data_buffer[25] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    // 10
    serial_data_buffer[26] = (uint8_t) (rc_channel_data[9] & 0xFF);
    serial_data_buffer[27] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    // 11
    serial_data_buffer[28] = (uint8_t) (rc_channel_data[10] & 0xFF);
    serial_data_buffer[29] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    // 12
    serial_data_buffer[30] = (uint8_t) (rc_channel_data[11] & 0xFF);
    serial_data_buffer[31] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    // CRC
    crc_mspv2_air = 0;
    for (i_rc_air = 3; i_rc_air < 32; i_rc_air++) {
        crc_mspv2_air = crc_dvb_s2_table[(crc_mspv2_air ^ serial_data_buffer[i_rc_air])] & 0xff;
    }
    serial_data_buffer[32] = crc_mspv2_air;
}

uint16_t generate_mavlinkv2_rc_overwrite(uint16_t *rc_channel_data) {
    mavlink_message_t message;
    mavlink_msg_rc_channels_override_pack(DB_MAVLINK_SYS_ID, 1, &message, 0, 0, rc_channel_data[0], rc_channel_data[1],
                                          rc_channel_data[2], rc_channel_data[3], rc_channel_data[4], rc_channel_data[5],
                                          rc_channel_data[6], rc_channel_data[7], rc_channel_data[8], rc_channel_data[9],
                                          rc_channel_data[10], rc_channel_data[11], 0, 0, 0, 0, 0, 0);
    return mavlink_msg_to_send_buffer(serial_data_buffer, &message);
}

/**
 * Sets the desired RC protocol that is outputted to serial port
 * @param new_serial_protocol 1:MSPv1, 2:MSPv2, 3:MAVLink v1, 4 or 5:MAVLink v2
 * @param use_sumd Use SUMD if set to 'Y'
 * @return
 */
void conf_rc_serial_protocol_air(int new_serial_protocol, char use_sumd){
    if (use_sumd == 'Y')
        serial_rc_protocol = RC_SERIAL_PROT_SUMD;
    else if (new_serial_protocol == 4 || new_serial_protocol == 5)
        serial_rc_protocol = RC_SERIAL_PROT_MAVLINKV2;
    else
        serial_rc_protocol = new_serial_protocol;
}

/**
 * Reads DroneBridge RC protocol and if crc is good we deserialize it and store the values (0-1000) inside rc_channels
 * @param db_rc_protocol_message A message formated as DroneBridge RC protocol
 * @return 1 if crc is valid and packet is good for further processing; -1 if packet has bad crc. Better skip it!
 * */
int deserialize_db_rc_protocol(uint8_t *db_rc_protocol_message) {
    crc_db_rc = 0x00;
    for (i_rc_air = 0; i_rc_air < 15; i_rc_air++) {
        crc_db_rc = crc_table_db_rc[(crc_db_rc ^ db_rc_protocol_message[i_rc_air])] & 0xff;
    }
    if (crc_db_rc == db_rc_protocol_message[15]){
        rc_channels[0] = db_rc_protocol_message[0] | ((db_rc_protocol_message[1] & 0x03) << 8);
        rc_channels[1] = ((db_rc_protocol_message[1] & 0xFC) >> 2) | ((db_rc_protocol_message[2] & 0x0F) << 6);
        rc_channels[2] = ((db_rc_protocol_message[2] & 0xF0) >> 4) | ((db_rc_protocol_message[3] & 0x3F) << 4);
        rc_channels[3] = ((db_rc_protocol_message[3] & 0xC0) >> 6) | (db_rc_protocol_message[4] << 2);

        rc_channels[4] = db_rc_protocol_message[5] | ((db_rc_protocol_message[6] & 0x03) << 8);
        rc_channels[5] = ((db_rc_protocol_message[6] & 0xFC) >> 2) | ((db_rc_protocol_message[7] & 0x0F) << 6);
        rc_channels[6] = ((db_rc_protocol_message[7] & 0xF0) >> 4) | ((db_rc_protocol_message[8] & 0x3F) << 4);
        rc_channels[7] = ((db_rc_protocol_message[8] & 0xC0) >> 6) | (db_rc_protocol_message[9] << 2);

        rc_channels[8] = db_rc_protocol_message[10] | ((db_rc_protocol_message[11] & 0x03) << 8);
        rc_channels[9] = ((db_rc_protocol_message[11] & 0xFC) >> 2) | ((db_rc_protocol_message[12] & 0x0F) << 6);
        rc_channels[10] = ((db_rc_protocol_message[12] & 0xF0) >> 4) | ((db_rc_protocol_message[13] & 0x3F) << 4);
        rc_channels[11] = ((db_rc_protocol_message[13] & 0xC0) >> 6) | (db_rc_protocol_message[14] << 2);
        return 1;
    } else {
        return -1; // packet is damaged
    }
}

/**
 * Init shared memory to write RC values before forwarding to FC
 */
void open_rc_rx_shm(){
    shm_rc_values = db_rc_values_memory_open();
}

/**
 * Takes a DroneBridge RC protocol message, checks it and generates a <valid message> to be sent over the serial port.
 * <valid message> protocol is specified via "conf_rc_protocol_air(int protocol)" (MSPv1, MSPv2, MAVLink v1, MAVLink v2)
 * @param db_rc_protocol
 * @return The number of bytes that the message for the serial port has. It depends on the picked serial protocol
 */
int generate_rc_serial_message(uint8_t *db_rc_protocol){
    if (deserialize_db_rc_protocol(db_rc_protocol) == 1){
        // adjust RC to 1000-2000 by adding 1000 to every channel received via DB-RC-Protocol
        rc_channels[0] += 1000; rc_channels[1] += 1000; rc_channels[2] += 1000; rc_channels[3] += 1000;
        rc_channels[4] += 1000; rc_channels[5] += 1000; rc_channels[6] += 1000; rc_channels[7] += 1000;
        rc_channels[8] += 1000; rc_channels[9] += 1000; rc_channels[10] += 1000; rc_channels[11] += 1000;
        // Update shared memory so that other modules/plugins can read from it
        shm_rc_values->ch[0] = rc_channels[0];shm_rc_values->ch[1] = rc_channels[1];shm_rc_values->ch[2] = rc_channels[2];
        shm_rc_values->ch[3] = rc_channels[3];shm_rc_values->ch[4] = rc_channels[4];shm_rc_values->ch[5] = rc_channels[5];
        shm_rc_values->ch[6] = rc_channels[6];shm_rc_values->ch[7] = rc_channels[7];shm_rc_values->ch[8] = rc_channels[8];
        shm_rc_values->ch[9] = rc_channels[9];shm_rc_values->ch[10] = rc_channels[10];shm_rc_values->ch[11] = rc_channels[11];

        if (serial_rc_protocol == RC_SERIAL_PROT_MSPV1){
            generate_msp(rc_channels);
            return 30;
        }else if (serial_rc_protocol == RC_SERIAL_PROT_MSPV2){
            generate_mspv2(rc_channels);
            return 33;
        }else if (serial_rc_protocol == RC_SERIAL_PROT_MAVLINKV1)
            perror(RED "MAVLink v1 RC packets unsupported - use SUMD" RESET "\n");
        else if (serial_rc_protocol == RC_SERIAL_PROT_MAVLINKV2)
            return generate_mavlinkv2_rc_overwrite(rc_channels);
        else if (serial_rc_protocol == RC_SERIAL_PROT_SUMD) {
            generate_sumd(rc_channels);
            return 29;
        }
    }
    return -1;
}
