#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "wifibroadcast.h"

/*
 * This file contains code licensed under GPL2. Code is part of EZ-WifiBroadcast project.
 */

typedef struct {
    uint32_t received_packet_cnt;
    uint32_t wrong_crc_cnt;
    int8_t current_signal_dbm;
    int8_t type; // 0 = Atheros, 1 = Ralink
    int signal_good;
} wifi_adapter_rx_status_t;

typedef struct {
    uint32_t received_packet_cnt;
    uint32_t wrong_crc_cnt;
    int8_t current_signal_dbm;
    int8_t type;
    int signal_good;
} wifi_adapter_rx_status_t_uplink;

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
    wifi_adapter_rx_status_t adapter[8];
} wifibroadcast_rx_status_t;

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
    wifi_adapter_rx_status_t adapter[8];
} wifibroadcast_rx_status_t_rc;

typedef struct {
    time_t last_update;
    uint8_t cpuload;
    uint8_t temp;
    uint32_t injected_block_cnt;
    uint32_t skipped_fec_cnt;
    uint32_t injection_fail_cnt;
    long long injection_time_block;
    uint16_t bitrate_kbit;
    uint16_t bitrate_measured_kbit;
    uint8_t cts;
    uint8_t undervolt; // 1=undervoltage
} wifibroadcast_rx_status_t_sysair;

typedef struct {
    time_t last_update;
    uint32_t injected_block_cnt;
    uint32_t skipped_fec_cnt;
    uint32_t injection_fail_cnt;
    long long injection_time_block;
} wifibroadcast_tx_status_t;
