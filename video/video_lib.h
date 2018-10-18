#pragma once

#include <stdint.h>
#include <stdlib.h>


typedef struct {
	int valid;
	int crc_correct;
	size_t len; //this is the actual length of the packet stored in data
	uint8_t *data;
} packet_buffer_t;

typedef struct {
	int block_num;
	packet_buffer_t *packet_buffer_list;
} block_buffer_t;

//this sits at the payload of the wifi packet (outside of FEC)
typedef struct {
    uint32_t sequence_number;
} __attribute__((packed)) video_packet_header_t;

//this sits at the data payload (which is usually right after the video_packet_header_t) (inside of FEC)
typedef struct {
    uint32_t data_length;
} __attribute__((packed)) payload_header_t;


packet_buffer_t *lib_alloc_packet_buffer_list(size_t num_packets, size_t packet_length);

//dronebridge_status_air_t *telemetry_wbc_status_memory_open_sysair(void);
