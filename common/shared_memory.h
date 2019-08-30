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

#include "db_protocol.h"

#ifndef CONTROL_STATUS_SHARED_MEMORY_H
#define CONTROL_STATUS_SHARED_MEMORY_H

#define MAX_ANTENNA_CNT 4

typedef struct {
    uint16_t ch[NUM_CHANNELS];
} __attribute__((packed)) db_rc_values_t;

typedef struct {
    struct timespec timestamp;
    uint16_t ch[NUM_CHANNELS];
} __attribute__((packed)) db_rc_overwrite_values_t;

typedef struct {
    uint32_t received_packet_cnt;
    uint32_t wrong_crc_cnt;
    int8_t current_signal_dbm; // main signal strength reported in radiotap header
    int8_t ant_signal_dbm[MAX_ANTENNA_CNT]; // signal strength of specific antenna of the adapter
    uint8_t num_antennas;
    uint8_t rate; // as reported by radiotap header (500kbps units, eg, 0x02=1Mbps)
    uint8_t lock_quality; // signal quality radiotap header
    uint8_t type; // 0 = Atheros, 1 = Ralink, 2 = Realtek
    char name[IFNAMSIZ];
} __attribute__((packed)) db_adapter_status;

typedef struct {
    time_t last_update; // video stream
    uint32_t received_block_cnt; // video stream
    uint32_t damaged_block_cnt; // video stream
    uint32_t lost_packet_cnt; // video stream
    uint32_t received_packet_cnt; // video stream
    uint32_t lost_per_block_cnt; // video stream
    uint32_t tx_restart_cnt; // video stream
    uint32_t kbitrate; // video stream
    uint32_t wifi_adapter_cnt; // video stream
    db_adapter_status adapter[8];
} __attribute__((packed)) db_gnd_status_t;

typedef struct {
    time_t last_update;
    uint32_t received_block_cnt;
    uint32_t damaged_block_cnt;
    uint32_t lost_packet_cnt;
    uint32_t received_packet_cnt;
    uint32_t lost_per_block_cnt;
    uint32_t tx_restart_cnt;
    uint32_t kbitrate;
    uint32_t wifi_adapter_cnt;
    db_adapter_status adapter[8];
} __attribute__((packed)) db_rc_status_t;

typedef struct {
    int encoding_time; // in microseconds
    uint8_t cpuload;
    uint8_t temp;
    uint32_t injected_block_cnt;
    uint32_t skipped_fec_cnt;
    uint32_t injection_fail_cnt;
    int injection_time_packet; // in microseconds
    uint32_t injected_packet_cnt;
    uint16_t bitrate_kbit;
    uint16_t bitrate_measured_kbit;
    uint8_t cts;
    uint8_t undervolt; // 1 = too low voltage
    uint32_t wifi_adapter_cnt; // video stream
    db_adapter_status adapter[8];
} __attribute__((packed)) db_uav_status_t;


db_gnd_status_t *db_gnd_status_memory_open(void);
db_rc_status_t *db_rc_status_memory_open(void);
db_uav_status_t *db_uav_status_memory_open(void);
db_rc_values_t *db_rc_values_memory_open(void);
db_rc_overwrite_values_t *db_rc_overwrite_values_memory_open(void);
void db_rc_values_memory_init(db_rc_values_t *rc_values);
void db_rc_overwrite_values_memory_init(db_rc_overwrite_values_t *rc_values);

#endif //CONTROL_STATUS_SHARED_MEMORY_H
