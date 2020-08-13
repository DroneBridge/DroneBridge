/*
 *   This file is part of DroneBridge: https://github.com/DroneBridge/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
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


#ifndef DRONEBRIDGE_FEC_SPEED_TEST_H
#define DRONEBRIDGE_FEC_SPEED_TEST_H


#define MAX_PACKET_LENGTH (DATA_UNI_LENGTH + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH)
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32
#define MAX_USER_PACKET_LENGTH 1450

static inline float TimeSpecToUSeconds(struct timespec *ts) {
    return (float) (ts->tv_sec + ts->tv_nsec / 1000.0);
}

#endif //DRONEBRIDGE_FEC_SPEED_TEST_H
