#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "../common/db_protocol.h"


typedef struct {
	int valid; // did we receive it or not (gets set to 1 if there is valid data inside data field)
	int crc_correct;
	uint len; // this is the actual length of the packet stored in data
	uint8_t *data; // this is video_packet_data_t
} packet_buffer_t;

typedef struct {
	int block_num;
	packet_buffer_t *packet_buffer_list;
} block_buffer_t;

// outside of FEC
typedef struct {
    uint32_t sequence_number;
} __attribute__((packed)) video_packet_header_t;

// protected by FEC
typedef struct {
	uint32_t data_length; // length of H264 video data
	uint8_t *data; // the H264 video data
} __attribute__((packed)) video_packet_data_t;

// the entire payload the gets sent with DB raw protocol and video transmission
typedef struct {
	video_packet_header_t video_packet_header;
	video_packet_data_t video_packet_data; // protected by FEC
} __attribute__((packed)) db_video_packet_t;

packet_buffer_t *lib_alloc_packet_buffer_list(size_t num_packets, size_t packet_length);
