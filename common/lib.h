#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "wifibroadcast.h"

typedef struct {
	uint32_t received_packet_cnt;
	uint32_t wrong_crc_cnt;
	int8_t current_signal_dbm;
	int8_t type; // 0 = Atheros, 1 = Ralink
} wifi_adapter_rx_status_t;

typedef struct {
	uint32_t received_packet_cnt;
	uint32_t wrong_crc_cnt;
	int8_t current_signal_dbm;
	int8_t type; // 0 = Atheros, 1 = Ralink
} wifi_adapter_rx_status_t_rc;

typedef struct {
	time_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[MAX_PENUMBRA_INTERFACES];
} wifibroadcast_rx_status_t;

typedef struct {
	time_t last_update;
	uint32_t received_block_cnt;
	uint32_t damaged_block_cnt;
	uint32_t lost_packet_cnt;
	uint32_t received_packet_cnt;
	uint32_t tx_restart_cnt;
	uint32_t kbitrate;
	uint32_t wifi_adapter_cnt;
	wifi_adapter_rx_status_t adapter[MAX_PENUMBRA_INTERFACES];
} wifibroadcast_rx_status_t_rc;

/*
typedef struct {
	int valid;
	int crc_correct;
	size_t len; //this is the actual length of the packet stored in data
	uint8_t *data;
} packet_buffer_t;

//this sits at the payload of the wifi packet (outside of FEC)
typedef struct {
    uint32_t sequence_number;
} __attribute__((packed)) wifi_packet_header_t;

//this sits at the data payload (which is usually right after the wifi_packet_header_t) (inside of FEC)
typedef struct {
    uint32_t data_length;
} __attribute__((packed)) payload_header_t;

// from OSD for rssitx and tx
typedef struct {
    wifibroadcast_rx_status_t *rx_status;
    wifibroadcast_rx_status_t_rc *rx_status_rc;
} telemetry_data_t;
*/
