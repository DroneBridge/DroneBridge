//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <stdbool.h>
#include <stdint.h>
#include "msp_serial.h"
#include "../db_crc/db_crc.h"

/**
 * This function is part of Cleanflight/iNAV.
 * Optimized for crc performance in the DroneBridge Project and ">" & "<" adjusted
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
bool mspSerialProcessReceivedData(mspPort_t *mspPort, uint8_t c)
{
    switch (mspPort->c_state) {
        default:
        case MSP_IDLE:      // Waiting for '$' character
            if (c == '$') {
                mspPort->mspVersion = MSP_V1;
                mspPort->c_state = MSP_HEADER_START;
            }
            else {
                return false;
            }
            break;

        case MSP_HEADER_START:  // Waiting for 'M' (MSPv1 / MSPv2_over_v1) or 'X' (MSPv2 native)
            switch (c) {
                case 'M':
                    mspPort->c_state = MSP_HEADER_M;
                    break;
                case 'X':
                    mspPort->c_state = MSP_HEADER_X;
                    break;
                default:
                    mspPort->c_state = MSP_IDLE;
                    break;
            }
            break;

        case MSP_HEADER_M:      // Waiting for '<'
            if (c == '>') {
                mspPort->offset = 0;
                mspPort->checksum1 = 0;
                mspPort->checksum2 = 0;
                mspPort->c_state = MSP_HEADER_V1;
            }
            else {
                mspPort->c_state = MSP_IDLE;
            }
            break;

        case MSP_HEADER_X:
            if (c == '>') {
                mspPort->offset = 0;
                mspPort->checksum2 = 0;
                mspPort->mspVersion = MSP_V2_NATIVE;
                mspPort->c_state = MSP_HEADER_V2_NATIVE;
            }
            else {
                mspPort->c_state = MSP_IDLE;
            }
            break;

        case MSP_HEADER_V1:     // Now receive v1 header (size/cmd), this is already checksummable
            mspPort->inBuf[mspPort->offset++] = c;
            mspPort->checksum1 ^= c;
            if (mspPort->offset == sizeof(mspHeaderV1_t)) {
                mspHeaderV1_t * hdr = (mspHeaderV1_t *)&mspPort->inBuf[0];
                // Check incoming buffer size limit
                if (hdr->size > MSP_PORT_INBUF_SIZE) {
                    mspPort->c_state = MSP_IDLE;
                }
                else if (hdr->cmd == MSP_V2_FRAME_ID) {
                    // MSPv1 payload must be big enough to hold V2 header + extra checksum
                    if (hdr->size >= sizeof(mspHeaderV2_t) + 1) {
                        mspPort->mspVersion = MSP_V2_OVER_V1;
                        mspPort->c_state = MSP_HEADER_V2_OVER_V1;
                    }
                    else {
                        mspPort->c_state = MSP_IDLE;
                    }
                }
                else {
                    mspPort->dataSize = hdr->size;
                    mspPort->cmdMSP = hdr->cmd;
                    mspPort->cmdFlags = 0;
                    mspPort->offset = 0;                // re-use buffer
                    mspPort->c_state = mspPort->dataSize > 0 ? MSP_PAYLOAD_V1 : MSP_CHECKSUM_V1;    // If no payload - jump to checksum byte
                }
            }
            break;

        case MSP_PAYLOAD_V1:
            mspPort->inBuf[mspPort->offset++] = c;
            mspPort->checksum1 ^= c;
            if (mspPort->offset == mspPort->dataSize) {
                mspPort->c_state = MSP_CHECKSUM_V1;
            }
            break;

        case MSP_CHECKSUM_V1:
            if (mspPort->checksum1 == c) {
                mspPort->c_state = MSP_COMMAND_RECEIVED;
            } else {
                mspPort->c_state = MSP_IDLE;
            }
            break;

        case MSP_HEADER_V2_OVER_V1:     // V2 header is part of V1 payload - we need to calculate both checksums now
            mspPort->inBuf[mspPort->offset++] = c;
            mspPort->checksum1 ^= c;
            mspPort->checksum2 = crc8_dvb_s2_table(mspPort->checksum2, c);
            if (mspPort->offset == (sizeof(mspHeaderV2_t) + sizeof(mspHeaderV1_t))) {
                mspHeaderV2_t * hdrv2 = (mspHeaderV2_t *)&mspPort->inBuf[sizeof(mspHeaderV1_t)];
                mspPort->dataSize = hdrv2->size;
                if (hdrv2->size > MSP_PORT_INBUF_SIZE) {
                    mspPort->c_state = MSP_IDLE;
                } else {
                    mspPort->cmdMSP = hdrv2->cmd;
                    mspPort->cmdFlags = hdrv2->flags;
                    mspPort->offset = 0;                // re-use buffer
                    mspPort->c_state = mspPort->dataSize > 0 ? MSP_PAYLOAD_V2_OVER_V1 : MSP_CHECKSUM_V2_OVER_V1;
                }
            }
            break;

        case MSP_PAYLOAD_V2_OVER_V1:
            mspPort->checksum2 = crc8_dvb_s2_table(mspPort->checksum2, c);
            mspPort->checksum1 ^= c;
            mspPort->inBuf[mspPort->offset++] = c;

            if (mspPort->offset == mspPort->dataSize) {
                mspPort->c_state = MSP_CHECKSUM_V2_OVER_V1;
            }
            break;

        case MSP_CHECKSUM_V2_OVER_V1:
            mspPort->checksum1 ^= c;
            if (mspPort->checksum2 == c) {
                mspPort->c_state = MSP_CHECKSUM_V1; // Checksum 2 correct - verify v1 checksum
            } else {
                mspPort->c_state = MSP_IDLE;
            }
            break;

        case MSP_HEADER_V2_NATIVE:
            mspPort->inBuf[mspPort->offset++] = c;
            mspPort->checksum2 = crc8_dvb_s2_table(mspPort->checksum2, c);
            if (mspPort->offset == sizeof(mspHeaderV2_t)) {
                mspHeaderV2_t * hdrv2 = (mspHeaderV2_t *)&mspPort->inBuf[0];
                if (hdrv2->size > MSP_PORT_INBUF_SIZE) {
                    mspPort->c_state = MSP_IDLE;
                } else {
                    mspPort->dataSize = hdrv2->size;
                    mspPort->cmdMSP = hdrv2->cmd;
                    mspPort->cmdFlags = hdrv2->flags;
                    mspPort->offset = 0;                // re-use buffer
                    mspPort->c_state = mspPort->dataSize > 0 ? MSP_PAYLOAD_V2_NATIVE : MSP_CHECKSUM_V2_NATIVE;
                }
            }
            break;

        case MSP_PAYLOAD_V2_NATIVE:
            mspPort->checksum2 = crc8_dvb_s2_table(mspPort->checksum2, c);
            mspPort->inBuf[mspPort->offset++] = c;

            if (mspPort->offset == mspPort->dataSize) {
                mspPort->c_state = MSP_CHECKSUM_V2_NATIVE;
            }
            break;

        case MSP_CHECKSUM_V2_NATIVE:
            if (mspPort->checksum2 == c) {
                mspPort->c_state = MSP_COMMAND_RECEIVED;
            } else {
                mspPort->c_state = MSP_IDLE;
            }
            break;
    }

    return true;
}