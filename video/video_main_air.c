/*   (c) 2015 befinitiv
 *   modified 2017 by Rodizio to work with EZ-Wifibroadcast
 *   modified 2018 by Wolfgang Christl (integration into DroneBridge https://github.com/DroneBridge)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <sys/time.h>
#include <stdbool.h>
#include <signal.h>
#include "fec.h"
#include "video_lib.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_send_receive.h"
#include "../common/wbc_lib.h"
#include "../common/shared_memory.h"
#include "../common/ccolors.h"

#define MAX_PACKET_LENGTH (DATA_UNI_LENGTH + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH)
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

bool keeprunning = true;
int skipfec = 0;
int block_cnt = 0;
uint8_t comm_id, frame_type, db_vid_seqnum = 0;
unsigned int num_interfaces = 0, num_data_block = 8, num_fec_block = 4, pack_size = 1024, bitrate_op = 6;
char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
db_socket raw_sockets[DB_MAX_ADAPTERS];

long long took_last = 0;
long long took = 0;
long long injection_time_now = 0;
long long injection_time_prev = 0;

typedef struct {
    uint32_t seq_nr;
    int fd;
    int curr_pb;
    packet_buffer_t *pbl;
} input_t;


long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long useconds = te.tv_sec*1000LL + te.tv_usec;
    return useconds;
}

void int_handler(int dummy){
    keeprunning = false;
}

/**
 *
 * @param seq_nr Video header sequence number
 * @param data_uni_to_ground Pointer to transmission buffer
 * @param packet_data Packet payload (FEC block or DATA block)
 * @param packet_length FEC block size
 * @param num_interfaces Length of raw_sockets[]
 * @param best_adapter Index of best wifi adapter inside raw_sockets[]
 * @return 0 if transmission success, -1 on fail
 */
int pb_transmit_packet(uint32_t seq_nr, struct data_uni *data_uni_to_ground, const uint8_t *packet_data,
        int packet_length, int num_interfaces, int best_adapter) {
    int i = 0;
    //add header outside of FEC but inside raw protocol payload buffer
    video_packet_header_t *wph = (video_packet_header_t*)(data_uni_to_ground);
    wph->sequence_number = seq_nr;

    //copy data to raw packet payload buffer
    memcpy(data_uni_to_ground + sizeof(video_packet_header_t), packet_data, (size_t) packet_length);

    uint16_t pay_length = packet_length + sizeof(video_packet_header_t);

    if (best_adapter == 5) {
        for(i = 0; i < num_interfaces; ++i) {
            if(send_packet_hp_div(&raw_sockets[i], DB_PORT_VIDEO, pay_length, update_seq_num(&db_vid_seqnum)) < 0)
                return -1;
        }
    } else {
        return send_packet_hp_div(&raw_sockets[best_adapter], DB_PORT_VIDEO, pay_length, update_seq_num(&db_vid_seqnum));
    }
    return -1;
}

/**
 * Takes payload data, generated FEC blocks and sends DATA and FEC blocks interleaved
 * @param pbl Array where the future payload data is located as blocks of data (payload is split into arrays)
 * @param data_uni_to_ground Pointer to transmission buffer
 * @param seq_nr Video header sequence number
 * @param packet_length FEC block size
 * @param data_packets_per_block
 * @param fec_packets_per_block
 * @param num_interfaces Length of raw_sockets[]
 * @param param_transmission_mode
 * @param db_uav_status Shared memory segment
 */
void pb_transmit_block(packet_buffer_t *pbl, struct data_uni *data_uni_to_ground, uint32_t *seq_nr, unsigned int packet_length,
        unsigned int data_packets_per_block, unsigned int fec_packets_per_block, int num_interfaces,
        int param_transmission_mode, db_uav_status_t *db_uav_status) {
    int i;
    uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][DATA_UNI_LENGTH];
    uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];

    for(i=0; i<data_packets_per_block; ++i) data_blocks[i] = pbl[i].data;

    if(fec_packets_per_block) {
        for(i=0; i<fec_packets_per_block; ++i) fec_blocks[i] = fec_pool[i];
        fec_encode(packet_length, data_blocks, data_packets_per_block, (unsigned char **)fec_blocks, fec_packets_per_block);
    }

    int di = 0;
    int fi = 0;
    uint32_t seq_nr_tmp = *seq_nr;
    long long prev_time = current_timestamp();
    int counterfec = 0;

    while(di < data_packets_per_block || fi < fec_packets_per_block) { //send data and FEC packets interleaved
        int best_adapter = 0;
        if(param_transmission_mode == 1) {
            int ac = db_uav_status->wifi_adapter_cnt;
            int best_dbm = -1000;

            // find out which card has best signal
            for(i=0; i<ac; ++i) {
                if (best_dbm < db_uav_status->adapter[i].current_signal_dbm) {
                    best_dbm = db_uav_status->adapter[i].current_signal_dbm;
                    best_adapter = i;
                }
            }
        } else {
            best_adapter = 5; // set to 5 to let transmit packet function know it shall transmit on all interfaces
        }

        if(di < data_packets_per_block) {
            if (pb_transmit_packet(seq_nr_tmp, data_uni_to_ground, data_blocks[di], packet_length,num_interfaces,best_adapter) < 0)
                db_uav_status->injection_fail_cnt++;
            seq_nr_tmp++;
            di++;
        }

        if(fi < fec_packets_per_block) {
            if (skipfec < 1) {
                if (pb_transmit_packet(seq_nr_tmp, data_uni_to_ground, fec_blocks[fi], packet_length,num_interfaces,best_adapter) < 0)
                    db_uav_status->injection_fail_cnt++;
            } else {
                if (counterfec % 2 == 0) {
                    if (pb_transmit_packet(seq_nr_tmp, data_uni_to_ground, fec_blocks[fi], packet_length,num_interfaces,best_adapter) < 0)
                        db_uav_status->injection_fail_cnt++;
                } else {
                }
                counterfec++;
            }
            seq_nr_tmp++;
            fi++;
        }
        skipfec--;
    }

    block_cnt++;
    db_uav_status->injected_block_cnt++;

    took_last = took;
    took = current_timestamp() - prev_time;

//	if (took > 50) fprintf(stdout,"write took %lldus\n", took);
    if (took > (packet_length * (data_packets_per_block + fec_packets_per_block)) / 1.5 ) { // we simply assume 1us per byte = 1ms per 1024 byte packet (not very exact ...)
//	    fprintf(stdout,"\nwrite took %lldus skipping FEC packets ...\n", took);
        skipfec=4;
        db_uav_status->skipped_fec_cnt = db_uav_status->skipped_fec_cnt + skipfec;
    }

    if(block_cnt % 50 == 0) {
        fprintf(stdout,"\t\t%d blocks sent, injection time per block %lldus, %d fecs skipped, "
                       "%d packet injections failed.          \r", block_cnt, db_uav_status->injection_time_block,
                       db_uav_status->skipped_fec_cnt, db_uav_status->injection_fail_cnt);
        fflush(stdout);
    }

    if (took < took_last) { // if we have a lower injection_time than last time, ignore
        took = took_last;
    }

    injection_time_now = current_timestamp();
    if (injection_time_now - injection_time_prev > 220) {
        injection_time_prev = current_timestamp();
        db_uav_status->injection_time_block = took;
        took=0;
        took_last=0;
    }
    *seq_nr += data_packets_per_block + fec_packets_per_block;

    //reset the length back
    for(i=0; i< data_packets_per_block; ++i) pbl[i].len = 0;

}

void process_command_line_args(int argc, char *argv[]){
    num_interfaces = 0, comm_id = DEFAULT_V2_COMMID;
    num_data_block = 8, num_fec_block = 4, pack_size = 1024, frame_type = 1;
    int c;
    while ((c = getopt (argc, argv, "n:c:d:r:f:b:t:")) != -1) {
        switch (c) {
            case 'n':
                strncpy(adapters[num_interfaces], optarg, IFNAMSIZ);
                num_interfaces++;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'd':
                num_data_block = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'r':
                num_fec_block = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'f':
                pack_size = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (uint8_t) strtol(optarg, NULL, 10);
            case 't':
                frame_type = (uint8_t) strtol(optarg, NULL, 10);
                break;
            default:
                printf("Based of Wifibroadcast by befinitiv, based on packetspammer by Andy Green.  Licensed under GPL2\n"
                       "This tool takes a data stream via the DroneBridge long range video port and outputs it via stdout, "
                       "UDP or TCP"
                       "\nIt uses the Reed-Solomon FEC code to repair lost or damaged packets."
                       "\n\n\t-n Name of a network interface that should be used to receive the stream. Must be in monitor "
                       "mode. Multiple interfaces supported by calling this option multiple times (-n inter1 -n inter2 -n interx)"
                       "\n\t-c <communication id> Choose a number from 0-255. Same on ground station and UAV!."
                       "\n\t-d Number of data packets in a block (default 8). Needs to match with tx."
                       "\n\t-r Number of FEC packets per block (default 4). Needs to match with tx."
                       "\n\t-f Bytes per packet (default %d. max %d). This is also the FEC "
                       "block size. Needs to match with tx."
                       "\n\t-b bit rate:\tin Mbps (1|2|5|6|9|11|12|18|24|36|48|54)\n\t\t(bitrate option only "
                       "supported with Ralink chipsets)"
                       "\n\t-t <1|2> DroneBridge v2 raw protocol packet/frame type: 1=RTS, 2=DATA (CTS protection)"
                        , 1024, DATA_UNI_LENGTH);
                abort();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    setpriority(PRIO_PROCESS, 0, -10);

    char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
    struct data_uni *data_uni_to_ground = (struct data_uni *)(monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);

    int pcnt = 0;
    input_t input;
    db_uav_status_t *db_uav_status = db_uav_status_memory_open();
    int param_min_packet_length = 24;

    process_command_line_args(argc, argv);

    if(pack_size > DATA_UNI_LENGTH) {
        fprintf(stderr, "ERROR; Packet length is limited to %d bytes (you requested %d bytes)\n", DATA_UNI_LENGTH, pack_size);
        return (1);
    }

    if(param_min_packet_length > pack_size) {
        fprintf(stderr, "ERROR; Minimum packet length is higher than maximum packet length (%d > %d)\n", param_min_packet_length, pack_size);
        return (1);
    }

    if(num_data_block > MAX_DATA_OR_FEC_PACKETS_PER_BLOCK || num_fec_block > MAX_DATA_OR_FEC_PACKETS_PER_BLOCK) {
        fprintf(stderr, "ERROR: Data and FEC packets per block are limited to %d (you requested %d data, %d FEC)\n",
                MAX_DATA_OR_FEC_PACKETS_PER_BLOCK, num_data_block, num_fec_block);
        return (1);
    }

    input.fd = STDIN_FILENO;
    input.seq_nr = 0;
    input.curr_pb = 0;
    input.pbl = lib_alloc_packet_buffer_list(num_data_block, MAX_PACKET_LENGTH);

    //prepare the buffers with headers
    int j = 0;
    for(j=0; j<num_data_block; ++j) {
        input.pbl[j].len = 0;
    }

    //initialize forward error correction
    fec_init();

    for (int k = 0; k < num_interfaces; ++k) {
        raw_sockets[k] = open_db_socket(adapters[k], comm_id, 'm', bitrate_op, DB_DIREC_GROUND, DB_PORT_VIDEO, frame_type);
    }
    printf(GRN "DB_VIDEO_AIR: started!" RESET "\n");
    while (keeprunning) {
        packet_buffer_t *pb = input.pbl + input.curr_pb;

        // if the buffer is fresh we add a payload header
        if(pb->len == 0) {
            pb->len += sizeof(payload_header_t); //make space for a length field (will be filled later)
        }

        ssize_t inl = read(input.fd, pb->data + pb->len, pack_size - pb->len); //read the data
        if(inl < 0 || inl > pack_size-pb->len) {
            perror("reading stdin");
            return 1;
        }

        if(inl == 0) { // EOF
            fprintf(stderr, "Warning: Lost connection to stdin. Please make sure that a data source is connected\n");
            usleep(5e5);
            continue;
        }

        pb->len += inl;

        // check if this packet is finished
        if(pb->len >= param_min_packet_length) {
            payload_header_t *ph = (payload_header_t*)pb->data;
            // write the length into the packet. this is needed since with fec we cannot use the wifi packet lentgh anymore.
            // We could also set the user payload to a fixed size but this would introduce additional latency since tx would need to wait until that amount of data has been received
            ph->data_length = (uint32_t) (pb->len - sizeof(payload_header_t));
            pcnt++;
            // check if this block is finished
            if(input.curr_pb == num_data_block-1) {
                pb_transmit_block(input.pbl, data_uni_to_ground, &(input.seq_nr), pack_size, num_data_block,
                        num_fec_block, num_interfaces, 2, db_uav_status);
                input.curr_pb = 0;
            } else {
                input.curr_pb++;
            }
        }
    }

    printf("ERROR: Broken socket!\n");
    return (0);
}
