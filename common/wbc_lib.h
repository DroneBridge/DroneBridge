#pragma once

#include <stdint.h>
#include <stdlib.h>

/*
 * This file contains code licensed under GPL2. Code is part of EZ-WifiBroadcast project.
 */

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	le16_to_cpu(x) (x)
#define	le32_to_cpu(x) (x)
#else
#define	le16_to_cpu(x) ((((x)&0xff)<<8)|(((x)&0xff00)>>8))
#define	le32_to_cpu(x) \
((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)&0xff000000)>>24))
#endif
#define	unlikely(x) (x)

#define	MAX_PENUMBRA_INTERFACES 8

typedef struct {
    uint32_t received_packet_cnt;
    uint32_t wrong_crc_cnt;
    int8_t current_signal_dbm;
    int8_t type; // 0 = Atheros, 1 = Ralink
    int signal_good;
} wifi_adapter_rx_status_t;

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
} db_video_rx_t;

typedef struct {
    time_t last_update;
    uint32_t injected_block_cnt;
    uint32_t skipped_fec_cnt;
    uint32_t injection_fail_cnt;
    long long injection_time_block;
} dronebridge_video_tx_t;
