//
// Created by Wolfgang Christl on 30.12.17.
// This file is part of DroneBridge
//
/*
 * This file contains code from Cleanflight & iNAV.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONTROL_STATUS_MSP_SERIAL_H
#define CONTROL_STATUS_MSP_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

#define MSP_MAX_HEADER_SIZE 9
#define MSP_V2_FRAME_ID 255
#define MSP_PORT_INBUF_SIZE 192
#define MAX_MSP_PORT_COUNT 3
#define MSP_PORT_DATAFLASH_BUFFER_SIZE 4096
#define MSP_PORT_OUTBUF_SIZE 512
#define MSP_VERSION_MAGIC_INITIALIZER { 'M', 'M', 'X' }


typedef enum {
    MSP_IDLE,
    MSP_HEADER_START,
    MSP_HEADER_M,
    MSP_HEADER_X,

    MSP_HEADER_V1,
    MSP_PAYLOAD_V1,
    MSP_CHECKSUM_V1,

    MSP_HEADER_V2_OVER_V1,
    MSP_PAYLOAD_V2_OVER_V1,
    MSP_CHECKSUM_V2_OVER_V1,

    MSP_HEADER_V2_NATIVE,
    MSP_PAYLOAD_V2_NATIVE,
    MSP_CHECKSUM_V2_NATIVE,

    MSP_COMMAND_RECEIVED,

    LTM_COMMAND_RECEIVED
} mspState_e;

typedef struct __attribute__((packed)) {
    uint8_t size;
    uint8_t cmd;
} mspHeaderV1_t;

typedef struct __attribute__((packed)) {
    uint16_t size;
} mspHeaderJUMBO_t;

typedef struct __attribute__((packed)) {
    uint8_t  flags;
    uint16_t cmd;
    uint16_t size;
} mspHeaderV2_t;

typedef enum {
    MSP_V1          = 0,
    MSP_V2_OVER_V1  = 1,
    MSP_V2_NATIVE   = 2,
    MSP_VERSION_COUNT
} mspVersion_e;

struct serialPort_s;
typedef struct mspPort_s {
    struct serialPort_s *port; // null when port unused.
    mspState_e c_state;
    uint8_t inBuf[MSP_PORT_INBUF_SIZE];
    uint_fast16_t offset;
    uint_fast16_t dataSize;
    mspVersion_e mspVersion;
    uint8_t cmdFlags;
    uint16_t cmdMSP;
    uint8_t checksum1;
    uint8_t checksum2;
} mspPort_t;

// return positive for ACK, negative on error, zero for no reply
typedef enum {
    MSP_RESULT_ACK = 1,
    MSP_RESULT_ERROR = -1,
    MSP_RESULT_NO_REPLY = 0
} mspResult_e;

// simple buffer-based serializer/deserializer without implicit size check
// little-endian encoding implemneted now
typedef struct sbuf_s {
    uint8_t *ptr;          // data pointer must be first (sbuff_t* is equivalent to uint8_t **)
    uint8_t *end;
} sbuf_t;

typedef struct mspPacket_s {
    sbuf_t buf;
    int16_t cmd;
    uint8_t flags;
    int16_t result;
} mspPacket_t;

//static mspPort_t mspPorts[MAX_MSP_PORT_COUNT];

bool mspSerialProcessReceivedData(mspPort_t *mspPort, uint8_t c);

#endif //CONTROL_STATUS_MSP_SERIAL_H
