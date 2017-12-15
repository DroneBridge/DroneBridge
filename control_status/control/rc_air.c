//
// Created by Wolfgang Christl on 10.12.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//
#include <stdint.h>
#include <stdio.h>
#include "rc_air.h"
#include "../common/db_protocol.h"
#include "../common/db_rc_crc.h"

int rc_protocol_air, i_rc_air;

uint8_t serial_data_buffer[1024] = {0}; // write the data for the serial port in here!

uint16_t rc_channels[DB_RC_NUM_CHANNELS] = {0};
uint8_t crc_mspv1_air = 0, crc_mspv2_air = 0, crc_db_rc = 0;
unsigned int tbl_idx_air;


void generate_msp(uint16_t *rc_channel_data) {
    serial_data_buffer[0] = 0x24;
    serial_data_buffer[1] = 0x4d;
    serial_data_buffer[2] = 0x3c;
    serial_data_buffer[3] = 0x18;   // payload size
    serial_data_buffer[4] = 0xc8;
    //Roll
    serial_data_buffer[5] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    serial_data_buffer[6] = (uint8_t) (rc_channel_data[0] & 0xFF);
    //Pitch
    serial_data_buffer[7] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    serial_data_buffer[8] = (uint8_t) (rc_channel_data[1] & 0xFF);
    //Yaw
    serial_data_buffer[9] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    serial_data_buffer[10] = (uint8_t) (rc_channel_data[2] & 0xFF);
    //Throttle
    serial_data_buffer[11] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    serial_data_buffer[12] = (uint8_t) (rc_channel_data[3] & 0xFF);
    //AUX 1
    serial_data_buffer[13] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    serial_data_buffer[14] = (uint8_t) (rc_channel_data[4] & 0xFF);
    //AUX 2
    serial_data_buffer[15] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    serial_data_buffer[16] = (uint8_t) (rc_channel_data[5] & 0xFF);
    //AUX 3
    serial_data_buffer[17] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    serial_data_buffer[18] = (uint8_t) (rc_channel_data[6] & 0xFF);
    //AUX 4
    serial_data_buffer[19] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    serial_data_buffer[20] = (uint8_t) (rc_channel_data[7] & 0xFF);
    //AUX 5
    serial_data_buffer[21] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    serial_data_buffer[22] = (uint8_t) (rc_channel_data[8] & 0xFF);
    //AUX 6
    serial_data_buffer[23] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    serial_data_buffer[24] = (uint8_t) (rc_channel_data[9] & 0xFF);
    //AUX 7
    serial_data_buffer[25] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    serial_data_buffer[26] = (uint8_t) (rc_channel_data[10] & 0xFF);
    //AUX 8
    serial_data_buffer[27] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    serial_data_buffer[28] = (uint8_t) (rc_channel_data[11] & 0xFF);
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
    //Roll
    serial_data_buffer[8] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    serial_data_buffer[9] = (uint8_t) (rc_channel_data[0] & 0xFF);
    //Pitch
    serial_data_buffer[10] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    serial_data_buffer[11] = (uint8_t) ( rc_channel_data[1] & 0xFF);
    //Yaw
    serial_data_buffer[12] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    serial_data_buffer[13] = (uint8_t) (rc_channel_data[2] & 0xFF);
    //Throttle
    serial_data_buffer[14] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    serial_data_buffer[15] = (uint8_t) (rc_channel_data[3] & 0xFF);
    //AUX 1
    serial_data_buffer[16] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    serial_data_buffer[17] = (uint8_t) (rc_channel_data[4] & 0xFF);
    //AUX 2
    serial_data_buffer[18] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    serial_data_buffer[19] = (uint8_t) (rc_channel_data[5] & 0xFF);
    //AUX 3
    serial_data_buffer[20] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    serial_data_buffer[21] = (uint8_t) (rc_channel_data[6] & 0xFF);
    //AUX 4
    serial_data_buffer[22] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    serial_data_buffer[23] = (uint8_t) (rc_channel_data[7] & 0xFF);
    //AUX 5
    serial_data_buffer[24] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    serial_data_buffer[25] = (uint8_t) (rc_channel_data[8] & 0xFF);
    //AUX 6
    serial_data_buffer[26] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    serial_data_buffer[27] = (uint8_t) (rc_channel_data[9] & 0xFF);
    //AUX 7
    serial_data_buffer[28] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    serial_data_buffer[29] = (uint8_t) (rc_channel_data[10] & 0xFF);
    //AUX 8
    serial_data_buffer[30] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    serial_data_buffer[31] = (uint8_t) (rc_channel_data[11] & 0xFF);
    // CRC
    crc_mspv2_air = 0;
    for (i_rc_air = 0; i_rc_air < 32; i_rc_air++) {
        tbl_idx_air = crc_mspv2_air ^ serial_data_buffer[i_rc_air];
        crc_mspv2_air = crc_dvb_s2_table[tbl_idx_air] & 0xff;
    }
    serial_data_buffer[32] = crc_mspv2_air;
}

/**
 * Sets the desired RC protocol that is outputted to serial port
 * @param new_rc_protocol 1:MSPv1, 2:MSPv2, 3:MAVLink v1, 4:MAVLink v2
 * @return
 */
int conf_rc_serial_protocol_air(int new_serial_protocol){
    rc_protocol_air = new_serial_protocol;
}

/**
 * Reads DroneBridge RC protocol and if crc is good we deserialize it and store the values (0-1000) inside rc_channels
 * @param db_rc_protocol_message A message formated as DroneBridge RC protocol
 * @return 1 if crc is valid and packet is good for further processing; -1 if packet has bad crc. Better skip it!
 * */
int deserialize_db_rc_protocol(uint8_t *db_rc_protocol_message) {
    crc_db_rc = 0x00;
    for (i_rc_air = 0; i_rc_air < 15; i_rc_air++) {
        tbl_idx_air = crc_db_rc ^ db_rc_protocol_message[i_rc_air];
        crc_db_rc = crc_table_db_rc[tbl_idx_air] & 0xff;
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

        // TODO remove
        printf( "%c[;H", 27 );
        printf("Roll:     %i          \n",rc_channels[0]);
        printf("Pitch:    %i          \n",rc_channels[1]);
        printf("Yaw:      %i          \n",rc_channels[2]);
        return 1;
    } else {
        return -1; // packet is damaged
    }
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
/*        for(i_rc_air = 0; i_rc_air < DB_RC_NUM_CHANNELS; i_rc_air++) {
            rc_channels[i_rc_air] += 1000;
        }*/
        if (rc_protocol_air == 1){
            generate_msp(rc_channels);
            return 30;
        }else if (rc_protocol_air == 2){
            generate_mspv2(rc_channels);
            return 33;
        }else if (rc_protocol_air == 3){
            // TODO: MAVLink v1 - seems it is not recommended to do RC override with MAVLink...
            perror("Unsupported for now");
        }else if (rc_protocol_air == 4){
            // TODO: generate MAVLink v2 - seems it is not recommended to do RC override with MAVLink...
            perror("Unsupported for now");
        }
    }
    return -1;
}
