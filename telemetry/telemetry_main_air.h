//
// Created by cyber on 08.06.18.
//

#ifndef CONTROL_STATUS_TELEMETRY_MAIN_AIR_H
#define CONTROL_STATUS_TELEMETRY_MAIN_AIR_H

const int UDP_buffersize = 512;
const int UDP_Port_TX = 1604;
char IP_TX[] = "192.168.3.2";

#define MAX_LTM_FRAME_SIZE 18
#define LTM_SIZE_ATT 7 // payload + crc
#define LTM_SIZE_STATUS 8 // payload + crc
#define LTM_SIZE_GPS 15 // payload + crc
#define transparent_chunksize 64

#endif //CONTROL_STATUS_TELEMETRY_MAIN_AIR_H
