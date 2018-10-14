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
#include "fec.h"
#include "video_lib.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_send_receive.h"
#include "../common/wbc_lib.h"
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

#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 2278
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

bool keeprunning = true;
int sock = 0;
int socks[4];
int skipfec = 0;
int block_cnt = 0;
int param_port = 0;
int frame_type = 1;
uint8_t comm_id;
int num_interfaces = 0, num_data_block = 8, num_fec_block = 4, pack_size = 1024, bitrate_op = 6;
char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];

long long took_last = 0;
long long took = 0;
long long injection_time_now = 0;
long long injection_time_prev = 0;
long long injection_time = 0;
long long pm_now = 0;
int flagHelp = 0;

typedef struct {
    int seq_nr;
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

int pb_transmit_packet(int seq_nr, uint8_t *packet_transmit_buffer, int packet_header_len, const uint8_t *packet_data,
        int packet_length, int num_interfaces, int param_transmission_mode, int best_adapter) {
    int i = 0;

    //add header outside of FEC
    wifi_packet_header_t *wph = (wifi_packet_header_t*)(packet_transmit_buffer + packet_header_len);
    wph->sequence_number = seq_nr;

    //copy data
    memcpy(packet_transmit_buffer + packet_header_len + sizeof(wifi_packet_header_t), packet_data, packet_length);

    int plen = packet_length + packet_header_len + sizeof(wifi_packet_header_t);

    if (best_adapter == 5) {
        for(i=0; i<num_interfaces; ++i) {
//	    if (write(socks[i], packet_transmit_buffer, plen) < 0 ) fprintf(stdout, "!");
            if (write(socks[i], packet_transmit_buffer, plen) < 0 ) return 1;
        }
    } else {
//	if (write(socks[best_adapter], packet_transmit_buffer, plen) < 0 ) fprintf(stdout, "!");
        if (write(socks[best_adapter], packet_transmit_buffer, plen) < 0 ) return 1;
    }
    return 0;
}




void pb_transmit_block(packet_buffer_t *pbl, int *seq_nr, int port, int packet_length, uint8_t *packet_transmit_buffer,
        int packet_header_len, int data_packets_per_block, int fec_packets_per_block, int num_interfaces,
        int param_transmission_mode, telemetry_data_t *td1) {
    int i;
    uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
    uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];

    for(i=0; i<data_packets_per_block; ++i) data_blocks[i] = pbl[i].data;

    if(fec_packets_per_block) {
        for(i=0; i<fec_packets_per_block; ++i) fec_blocks[i] = fec_pool[i];
        fec_encode(packet_length, data_blocks, data_packets_per_block, (unsigned char **)fec_blocks, fec_packets_per_block);
    }

    uint8_t *pb = packet_transmit_buffer;
    pb += packet_header_len;

    int di = 0;
    int fi = 0;
    int seq_nr_tmp = *seq_nr;
    long long prev_time = current_timestamp();
    int counterfec = 0;

    while(di < data_packets_per_block || fi < fec_packets_per_block) { //send data and FEC packets interleaved
        int best_adapter = 0;
        if(param_transmission_mode == 1) {
            int i;
            int ac = td1->rx_status->wifi_adapter_cnt;
            int best_dbm = -1000;

            // find out which card has best signal
            for(i=0; i<ac; ++i) {
                if (best_dbm < td1->rx_status->adapter[i].current_signal_dbm) {
                    best_dbm = td1->rx_status->adapter[i].current_signal_dbm;
                    best_adapter = i;
                }
            }
//    		printf ("bestadapter: %d (%d dbm)\n",best_adapter, best_dbm);
        } else {
            best_adapter = 5; // set to 5 to let transmit packet function know it shall transmit on all interfaces
        }

        if(di < data_packets_per_block) {
            if (pb_transmit_packet(seq_nr_tmp, packet_transmit_buffer, packet_header_len, data_blocks[di], packet_length,num_interfaces, param_transmission_mode,best_adapter)) td1->tx_status->injection_fail_cnt++;
            seq_nr_tmp++;
            di++;
        }

        if(fi < fec_packets_per_block) {
            if (skipfec < 1) {
                if (pb_transmit_packet(seq_nr_tmp, packet_transmit_buffer, packet_header_len, fec_blocks[fi], packet_length,num_interfaces,param_transmission_mode,best_adapter)) td1->tx_status->injection_fail_cnt++;
            } else {
                if (counterfec % 2 == 0) {
                    if (pb_transmit_packet(seq_nr_tmp, packet_transmit_buffer, packet_header_len, fec_blocks[fi], packet_length,num_interfaces,param_transmission_mode,best_adapter)) td1->tx_status->injection_fail_cnt++;
                } else {
//			   fprintf(stdout,"not transmitted\n");
                }
                counterfec++;
            }
            seq_nr_tmp++;
            fi++;
        }
        skipfec--;
    }

    block_cnt++;
    td1->tx_status->injected_block_cnt++;

    took_last = took;
    took = current_timestamp() - prev_time;

//	if (took > 50) fprintf(stdout,"write took %lldus\n", took);
    if (took > (packet_length * (data_packets_per_block + fec_packets_per_block)) / 1.5 ) { // we simply assume 1us per byte = 1ms per 1024 byte packet (not very exact ...)
//	    fprintf(stdout,"\nwrite took %lldus skipping FEC packets ...\n", took);
        skipfec=4;
        td1->tx_status->skipped_fec_cnt = td1->tx_status->skipped_fec_cnt + skipfec;
    }

    if(block_cnt % 50 == 0) {
        fprintf(stdout,"\t\t%d blocks sent, injection time per block %lldus, %d fecs skipped, %d packet injections failed.          \r", block_cnt,td1->tx_status->injection_time_block,td1->tx_status->skipped_fec_cnt,td1->tx_status->injection_fail_cnt);
        fflush(stdout);
    }

    if (took < took_last) { // if we have a lower injection_time than last time, ignore
        took = took_last;
    }

    injection_time_now = current_timestamp();
    if (injection_time_now - injection_time_prev > 220) {
        injection_time_prev = current_timestamp();
        td1->tx_status->injection_time_block = took;
        took=0;
        took_last=0;
    }


    *seq_nr += data_packets_per_block + fec_packets_per_block;

    //reset the length back
    for(i=0; i< data_packets_per_block; ++i) pbl[i].len = 0;

}

void status_memory_init(dronebridge_video_tx_t *s) {
    s->last_update = 0;
    s->injected_block_cnt = 0;
    s->skipped_fec_cnt = 0;
    s->injection_fail_cnt = 0;
    s->injection_time_block = 0;
}



void telemetry_init(telemetry_data_t *td) {
    td->rx_status = telemetry_wbc_status_memory_open();
    td->tx_status = telemetry_wbc_status_memory_open_tx();
}

void process_command_line_args(int argc, char *argv[]){
    num_interfaces = 0, comm_id = DEFAULT_V2_COMMID;
    num_data_block = 8, num_fec_block = 4, pack_size = 1024;
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
                       "\n\t-t <1|2> DroneBridge v2 raw protocol packet type: 1=RTS, 2=DATA"
                        , 1024, MAX_USER_PACKET_LENGTH);
                abort();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    setpriority(PRIO_PROCESS, 0, -10);

    char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
    db_socket raw_sockets[DB_MAX_ADAPTERS];
    char fBrokenSocket = 0;
    int pcnt = 0;
    uint8_t packet_transmit_buffer[MAX_PACKET_LENGTH];
    size_t packet_header_length = 0;
    input_t input;

    int param_min_packet_length = 24;

    process_command_line_args(argc, argv);

    if(pack_size > MAX_USER_PACKET_LENGTH) {
        fprintf(stderr, "ERROR; Packet length is limited to %d bytes (you requested %d bytes)\n", MAX_USER_PACKET_LENGTH, pack_size);
        return (1);
    }

    if(param_min_packet_length > pack_size) {
        fprintf(stderr, "ERROR; Minimum packet length is higher than maximum packet length (%d > %d)\n", param_min_packet_length, pack_size);
        return (1);
    }

    if(num_data_block > MAX_DATA_OR_FEC_PACKETS_PER_BLOCK || num_fec_block > MAX_DATA_OR_FEC_PACKETS_PER_BLOCK) {
        fprintf(stderr, "ERROR: Data and FEC packets per block are limited to %d (you requested %d data, %d FEC)\n", MAX_DATA_OR_FEC_PACKETS_PER_BLOCK, num_data_block, num_fec_block);
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
    // initialize telemetry shared mem for rssi based transmission (-y 1)
    dronebridge_video_tx_t td;
    telemetry_init(&td);

    for (int k = 0; k < num_interfaces; ++k) {
        raw_sockets[k] = open_db_socket(adapters[k], comm_id, 'm', bitrate_op, DB_DIREC_GROUND, DB_PORT_VIDEO);
    }

    while (!fBrokenSocket && keeprunning) {
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
            ph->data_length = pb->len - sizeof(payload_header_t);
            pcnt++;
            // check if this block is finished
            if(input.curr_pb == num_data_block-1) {
                pb_transmit_block(input.pbl, &(input.seq_nr), param_port, pack_size, packet_transmit_buffer,
                        packet_header_length, num_data_block, num_fec_block, num_interfaces, param_transmission_mode, &td);
                input.curr_pb = 0;
            } else {
                input.curr_pb++;
            }
        }
    }

    printf("ERROR: Broken socket!\n");
    return (0);
}
