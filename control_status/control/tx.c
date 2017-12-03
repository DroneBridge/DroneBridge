//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <arpa/inet.h>
#include "parameter.h"
#include "../common/db_raw_send.h"


int rc_protocol;
struct ifreq if_idx;
struct ifreq if_mac;
char interfaceName[IFNAMSIZ];

char mode;
uint8_t crcS2, crc8;

// pointing right into the sockets send buffer for max performance
struct data_uni *monitor_databuffer = (struct data_uni *) (monitor_framebuffer + RADIOTAP_LENGTH + MSP_V2_DATA_LENGTH);

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
    crc8 = 0;
    for (int i = 3; i < 33; i++) {
        crc8 ^= (monitor_databuffer->bytes[i] & 0xFF);
    }
    monitor_databuffer->bytes[33] = crc8;
}

uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a)
{
    crc ^= a;
    for (int ii = 0; ii < 8; ++ii) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0xD5;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
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
    crcS2 = 0;
    for(int i = 3; i < 36; i++)
        crcS2 = crc8_dvb_s2(crcS2, monitor_databuffer->bytes[i]);
    monitor_databuffer->bytes[36] = crcS2;
}

void generate_mavlink_v1(unsigned short newJoystickData[NUM_CHANNELS]) {
    // TODO
}

void generate_db_rc_packet(unsigned short new_joystick_data[NUM_CHANNELS]){
    // TODO
}

/**
 * Sets the desired RC protocol.
 * @param new_rc_protocol 1:MSPv1, 2:MSPv3, 3:MAVLinkV1, 4=MAVLinkV2
 * @return
 */
int conf_rc_protocol(int new_rc_protocol){
    rc_protocol = new_rc_protocol;
}

/**
 * Takes the channel data (1000-2000) and builds valid packets from it. Depending on specified RC protocol.
 * @param contData Values in between 1000 and 2000
 * @return
 */
int send_rc_packet(unsigned short contData[]) {
    // TODO check for RC overwrite!
    if (rc_protocol == 1){
        generate_msp(contData);
        send_packet_hp(DB_PORT_CONTROLLER, MSP_DATA_LENTH); // TODO: DB_PORT_RC
    }else if (rc_protocol == 2){
        generate_mspv2(contData);
        send_packet_hp(DB_PORT_CONTROLLER, MSP_V2_DATA_LENGTH); // TODO: DB_PORT_RC
    }else if (rc_protocol == 3){
        generate_mavlink_v1(contData);
        // TODO: set db_payload length, send MAVLink
    }

//    printf( "%c[;H", 27 );
//    printf("\n");
//    for(int i = (RADIOTAP_LENGTH + DB80211_HEADER_LENGTH);i< (RADIOTAP_LENGTH + DB80211_HEADER_LENGTH + MSP_V2_DATA_LENTH);i++){
//        printf(" %02x",monitor_databuffer[i]);
//    }

    return 0;
}
