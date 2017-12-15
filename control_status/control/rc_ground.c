//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <arpa/inet.h>
#include <stdint.h>
#include "parameter.h"
#include "../common/db_raw_send_receive.h"
#include "../common/db_rc_crc.h"
#include "../common/shared_memory.h"
#include "../common/db_protocol.h"


int rc_protocol;
uint8_t crc_mspv2, crc8, rc_seq_number = 0;
crc_t crc_rc;
int i_crc, i_rc;
unsigned int rc_crc_tbl_idx, mspv2_tbl_idx;
db_rc_values *shm_rc_values = NULL;
struct timeval timecheck;

// pointing right into the sockets send buffer for max performance
struct data_uni *monitor_databuffer = (struct data_uni *) (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);

// could do this with two for-loops but hardcoded is faster and number of aux channels won't change anyways
void generate_msp(unsigned short *newJoystickData) {
    monitor_databuffer->bytes[0] = 0x24;
    monitor_databuffer->bytes[1] = 0x4d;
    monitor_databuffer->bytes[2] = 0x3c;
    monitor_databuffer->bytes[3] = 0x1c;
    monitor_databuffer->bytes[4] = 0xc8;
    //Roll
    monitor_databuffer->bytes[5] = (uint8_t) ((newJoystickData[0] >> 8) & 0xFF);
    monitor_databuffer->bytes[6] = (uint8_t) (newJoystickData[0] & 0xFF);
    //Pitch
    monitor_databuffer->bytes[7] = (uint8_t) ((newJoystickData[1] >> 8) & 0xFF);
    monitor_databuffer->bytes[8] = (uint8_t) (newJoystickData[1] & 0xFF);
    //Yaw
    monitor_databuffer->bytes[9] = (uint8_t) ((newJoystickData[2] >> 8) & 0xFF);
    monitor_databuffer->bytes[10] = (uint8_t) (newJoystickData[2] & 0xFF);
    //Throttle
    monitor_databuffer->bytes[11] = (uint8_t) ((newJoystickData[3] >> 8) & 0xFF);
    monitor_databuffer->bytes[12] = (uint8_t) (newJoystickData[3] & 0xFF);
    //AUX 1
    monitor_databuffer->bytes[13] = (uint8_t) ((newJoystickData[4] >> 8) & 0xFF);
    monitor_databuffer->bytes[14] = (uint8_t) (newJoystickData[4] & 0xFF);
    //AUX 2
    monitor_databuffer->bytes[15] = (uint8_t) ((newJoystickData[5] >> 8) & 0xFF);
    monitor_databuffer->bytes[16] = (uint8_t) (newJoystickData[5] & 0xFF);
    //AUX 3
    monitor_databuffer->bytes[17] = (uint8_t) ((newJoystickData[6] >> 8) & 0xFF);
    monitor_databuffer->bytes[18] = (uint8_t) (newJoystickData[6] & 0xFF);
    //AUX 4
    monitor_databuffer->bytes[19] = (uint8_t) ((newJoystickData[7] >> 8) & 0xFF);
    monitor_databuffer->bytes[20] = (uint8_t) (newJoystickData[7] & 0xFF);
    //AUX 5
    monitor_databuffer->bytes[21] = (uint8_t) ((newJoystickData[8] >> 8) & 0xFF);
    monitor_databuffer->bytes[22] = (uint8_t) (newJoystickData[8] & 0xFF);
    //AUX 6
    monitor_databuffer->bytes[23] = (uint8_t) ((newJoystickData[9] >> 8) & 0xFF);
    monitor_databuffer->bytes[24] = (uint8_t) (newJoystickData[9] & 0xFF);
    //AUX 7
    monitor_databuffer->bytes[25] = (uint8_t) ((newJoystickData[10] >> 8) & 0xFF);
    monitor_databuffer->bytes[26] = (uint8_t) (newJoystickData[10] & 0xFF);
    //AUX 8
    monitor_databuffer->bytes[27] = (uint8_t) ((newJoystickData[11] >> 8) & 0xFF);
    monitor_databuffer->bytes[28] = (uint8_t) (newJoystickData[11] & 0xFF);
    //AUX 9
    monitor_databuffer->bytes[29] = (uint8_t) ((newJoystickData[12] >> 8) & 0xFF);
    monitor_databuffer->bytes[30] = (uint8_t) (newJoystickData[12] & 0xFF);
    //AUX 10
    monitor_databuffer->bytes[31] = (uint8_t) ((newJoystickData[13] >> 8) & 0xFF);
    monitor_databuffer->bytes[32] = (uint8_t) (newJoystickData[13] & 0xFF);
    // CRC
    crc8 = 0; // not really a crc more a checksum
    for (int i = 3; i < 33; i++) {
        crc8 ^= (monitor_databuffer->bytes[i] & 0xFF);
    }
    monitor_databuffer->bytes[33] = crc8;
}

void generate_mspv2(unsigned short *newJoystickData) {
    monitor_databuffer->bytes[0] = 0x24;
    monitor_databuffer->bytes[1] = 0x58;
    monitor_databuffer->bytes[2] = 0x3c;
    monitor_databuffer->bytes[3] = 0x00; // flag
    monitor_databuffer->bytes[4] = 0xc8; // function
    monitor_databuffer->bytes[5] = 0x00; // function
    monitor_databuffer->bytes[6] = 0x1c; // payload size
    monitor_databuffer->bytes[7] = 0x00; // payload size
    //Roll
    monitor_databuffer->bytes[8] = (uint8_t) ((newJoystickData[0] >> 8) & 0xFF);
    monitor_databuffer->bytes[9] = (uint8_t) (newJoystickData[0] & 0xFF);
    //Pitch
    monitor_databuffer->bytes[10] = (uint8_t) ((newJoystickData[1] >> 8) & 0xFF);
    monitor_databuffer->bytes[11] = (uint8_t) ( newJoystickData[1] & 0xFF);
    //Yaw
    monitor_databuffer->bytes[12] = (uint8_t) ((newJoystickData[2] >> 8) & 0xFF);
    monitor_databuffer->bytes[13] = (uint8_t) (newJoystickData[2] & 0xFF);
    //Throttle
    monitor_databuffer->bytes[14] = (uint8_t) ((newJoystickData[3] >> 8) & 0xFF);
    monitor_databuffer->bytes[15] = (uint8_t) (newJoystickData[3] & 0xFF);
    //AUX 1
    monitor_databuffer->bytes[16] = (uint8_t) ((newJoystickData[4] >> 8) & 0xFF);
    monitor_databuffer->bytes[17] = (uint8_t) (newJoystickData[4] & 0xFF);
    //AUX 2
    monitor_databuffer->bytes[18] = (uint8_t) ((newJoystickData[5] >> 8) & 0xFF);
    monitor_databuffer->bytes[19] = (uint8_t) (newJoystickData[5] & 0xFF);
    //AUX 3
    monitor_databuffer->bytes[20] = (uint8_t) ((newJoystickData[6] >> 8) & 0xFF);
    monitor_databuffer->bytes[21] = (uint8_t) (newJoystickData[6] & 0xFF);
    //AUX 4
    monitor_databuffer->bytes[22] = (uint8_t) ((newJoystickData[7] >> 8) & 0xFF);
    monitor_databuffer->bytes[23] = (uint8_t) (newJoystickData[7] & 0xFF);
    //AUX 5
    monitor_databuffer->bytes[24] = (uint8_t) ((newJoystickData[8] >> 8) & 0xFF);
    monitor_databuffer->bytes[25] = (uint8_t) (newJoystickData[8] & 0xFF);
    //AUX 6
    monitor_databuffer->bytes[26] = (uint8_t) ((newJoystickData[9] >> 8) & 0xFF);
    monitor_databuffer->bytes[27] = (uint8_t) (newJoystickData[9] & 0xFF);
    //AUX 7
    monitor_databuffer->bytes[28] = (uint8_t) ((newJoystickData[10] >> 8) & 0xFF);
    monitor_databuffer->bytes[29] = (uint8_t) (newJoystickData[10] & 0xFF);
    //AUX 8
    monitor_databuffer->bytes[30] = (uint8_t) ((newJoystickData[11] >> 8) & 0xFF);
    monitor_databuffer->bytes[31] = (uint8_t) (newJoystickData[11] & 0xFF);
    //AUX 9
    monitor_databuffer->bytes[32] = (uint8_t) ((newJoystickData[12] >> 8) & 0xFF);
    monitor_databuffer->bytes[33] = (uint8_t) (newJoystickData[12] & 0xFF);
    //AUX 10
    monitor_databuffer->bytes[34] = (uint8_t) ((newJoystickData[13] >> 8) & 0xFF);
    monitor_databuffer->bytes[35] = (uint8_t) (newJoystickData[13] & 0xFF);
    // CRC
    crc_mspv2 = 0;
    for (int i = 0; i < 36; i++) {
        mspv2_tbl_idx = crc_mspv2 ^ monitor_databuffer->bytes[i];
        crc_mspv2 = crc_dvb_s2_table[mspv2_tbl_idx] & 0xff;
    }
    monitor_databuffer->bytes[36] = crc_mspv2;
}

void generate_mavlink_v1(unsigned short newJoystickData[NUM_CHANNELS]) {
    // TODO
}

void generate_db_rc_message(uint16_t channels[NUM_CHANNELS]){
    // Security check. Cap values. Poorly calibrated joysticks might lead to unwanted behavior!
    for (i_crc = 0; i_crc < DB_RC_NUM_CHANNELS; i_crc++){
        if (channels[i_crc] < 1000){
            channels[i_crc] = 1000;
        } else if (channels[i_crc] > 2000){
            channels[i_crc] = 2000;
        }
        channels[i_crc] -= 1000;
    }

    monitor_databuffer->bytes[0] = (uint8_t) (channels[0] & 0xFF);
    monitor_databuffer->bytes[1] = (uint8_t) (((channels[0] & 0x0300) >> 8) | ((channels[1] & 0x3F) << 2));
    monitor_databuffer->bytes[2] = (uint8_t) (((channels[1] & 0x03C0) >> 6) | ((channels[2] & 0x0F) << 4));
    monitor_databuffer->bytes[3] = (uint8_t) (((channels[2] & 0x03F0) >> 4) | ((channels[3] & 0x03) << 6));
    monitor_databuffer->bytes[4] = (uint8_t) ((channels[3] & 0x03FC) >> 2);

    monitor_databuffer->bytes[5] = (uint8_t) (channels[4] & 0xFF);
    monitor_databuffer->bytes[6] = (uint8_t) (((channels[4] & 0x0300) >> 8) | ((channels[5] & 0x3F) << 2));
    monitor_databuffer->bytes[7] = (uint8_t) (((channels[5] & 0x03C0) >> 6) | ((channels[6] & 0x0F) << 4));
    monitor_databuffer->bytes[8] = (uint8_t) (((channels[6] & 0x03F0) >> 4) | ((channels[7] & 0x03) << 6));
    monitor_databuffer->bytes[9] = (uint8_t) ((channels[7] & 0x03FC) >> 2);

    monitor_databuffer->bytes[10] = (uint8_t) (channels[8] & 0xFF);
    monitor_databuffer->bytes[11] = (uint8_t) (((channels[8] & 0x0300) >> 8) | ((channels[9] & 0x3F) << 2));
    monitor_databuffer->bytes[12] = (uint8_t) (((channels[9] & 0x03C0) >> 6) | ((channels[10] & 0x0F) << 4));
    monitor_databuffer->bytes[13] = (uint8_t) (((channels[10] & 0x03F0) >> 4) | ((channels[11] & 0x03) << 6));
    monitor_databuffer->bytes[14] = (uint8_t) ((channels[11] & 0x03FC) >> 2);
    crc_rc = 0x00;
    for (i_crc = 0; i_crc < 15; i_crc++) {
        rc_crc_tbl_idx = crc_rc ^ monitor_databuffer->bytes[i_crc];
        crc_rc = crc_table_db_rc[rc_crc_tbl_idx] & 0xff;
    }
    monitor_databuffer->bytes[15] = (uint8_t) (crc_rc & 0xff);
}

/**
 * Sets the desired RC protocol.
 * @param new_rc_protocol 1:MSPv1, 2:MSPv2, 3:MAVLink v1, 4:MAVLink v2, 5:DB-RC
 * @return
 */
int conf_rc_protocol(int new_rc_protocol){
    rc_protocol = new_rc_protocol;
}

/**
 * Init shared memory to write RC values before sending
 */
void open_rc_tx_shm(){
    shm_rc_values = db_rc_values_memory_open();
}

/**
 * Takes the channel data (1000-2000) and builds valid packets from it. Depending on specified RC protocol.
 * @param contData Values in between 1000 and 2000
 * @return
 */
int send_rc_packet(uint16_t channel_data[]) {
    // TODO: check for RC overwrite!
    if (rc_seq_number == 255){
        rc_seq_number = 0;
    } else {
        rc_seq_number++;
    }
    // Update shared memory so status module can read RC values
    for(i_rc = 0; i_rc < NUM_CHANNELS; i_rc++) {
        shm_rc_values->ch[i_rc] = channel_data[i_rc];
    }

    if (rc_protocol == 1){
        generate_msp(channel_data);
        send_packet_hp(DB_PORT_CONTROLLER, MSP_DATA_LENTH, rc_seq_number);
    }else if (rc_protocol == 2){
        generate_mspv2(channel_data);
        send_packet_hp(DB_PORT_CONTROLLER, MSP_V2_DATA_LENGTH, rc_seq_number);
    }else if (rc_protocol == 3){
        generate_mavlink_v1(channel_data);
        // TODO: set db_payload length, send MAVLink
    }else if (rc_protocol == 4){
        // TODO: generate MAVLink v2
    } else if (rc_protocol == 5){
        generate_db_rc_message(channel_data);
        printf( "%c[;H", 27 );
        printf("CH8:     %i          \n",channel_data[8]);
        printf("CH9:     %i          \n",channel_data[9]);
        printf("CH10:    %i          \n",channel_data[10]);
        send_packet_hp(DB_PORT_RC, DB_RC_DATA_LENGTH, rc_seq_number);
    }
    return 0;
}
