/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
 *   integrated into the DroneBridge project by Wolfgang Christl
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "fec.h"
#include "radiotap.h"
#include "video_main_gnd.h"
#include "lib.h"
#include "../common/db_get_ip.h"
#include "../common/shared_memory.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/ccolors.h"

#define DB_MAX_ADAPTERS 4
#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 2278
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32

bool volatile keeprunning = true;
bool pass_through = false, udp_enabled = true;
char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
int packetcounter[DB_MAX_ADAPTERS];
int dbm[DB_MAX_ADAPTERS];
int num_interfaces, udp_socket, shID;
struct ieee80211_radiotap_iterator rti;
uint8_t comm_id, num_data_block, num_fec_block, pack_size;
wifibroadcast_rx_status_t *wbc_rx_status;
struct sockaddr_in client_video_addr;

int param_block_buffers = 1;
long long dbm_ts_prev[DB_MAX_ADAPTERS];
long long dbm_ts_now[DB_MAX_ADAPTERS];
long long prev_time = 0;
long long now = 0;
int bytes_written = 0;
int packets_missing;
int packets_missing_last;
int dbm_last[DB_MAX_ADAPTERS];
int packetcounter_last[DB_MAX_ADAPTERS];
long long pm_prev_time = 0;
long long pm_now = 0;
int max_block_num = -1;

void intHandler(int dummy){
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]){
    num_interfaces = 0;
    num_data_block = 8, num_fec_block = 4, pack_size = 1024;
    int c;
    while ((c = getopt (argc, argv, "n:c:b:r:f:p:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(adapters[num_interfaces], optarg, IFNAMSIZ);
                num_interfaces++;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'b':
                num_data_block = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'r':
                num_fec_block = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'f':
                pack_size = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'p':
                if (*optarg == 'Y')
                    pass_through = true;
                break;
            default:
                printf("This tool takes a data stream via the DroneBridge long range video port and outputs it via stdout, "
                       "UDP or TCP"
                       "\nIt uses the Reed-Solomon FEC code to repair lost or damaged packets."
                       "\n\n\t-n Name of a network interface that should be used to receive the stream. Must be in monitor "
                       "mode. Multiple interfaces supported by calling this option multiple times (-n inter1 -n inter2 -n interx)"
                       "\n\t-c <communication id> Choose a number from 0-255. Same on ground station and UAV!."
                       "\n\t-b Number of data packets in a block (default 8). Needs to match with tx."
                       "\n\t-r Number of FEC packets per block (default 4). Needs to match with tx."
                       "\n\t-f Bytes per packet (default %d. max %d). This is also the FEC "
                       "block size. Needs to match with tx.", 1024, MAX_USER_PACKET_LENGTH);
                abort();
        }
    }
}

void init_outputs(){
    int app_port_video = APP_PORT_VIDEO;
    if (pass_through) app_port_video = APP_PORT_VIDEO_FEC;
    if (udp_enabled){
        udp_socket = socket (AF_INET, SOCK_DGRAM, 0);
        client_video_addr.sin_family = AF_INET;
        client_video_addr.sin_addr.s_addr = inet_addr("192.168.2.2");
        client_video_addr.sin_port = htons(app_port_video);
    }

}

void block_buffer_list_reset(block_buffer_t *block_buffer_list, size_t block_buffer_list_len, int block_buffer_len) {
    int i;
    block_buffer_t *rb = block_buffer_list;

    for(i=0; i<block_buffer_list_len; ++i) {
        rb->block_num = -1;
        int j;
        packet_buffer_t *p = rb->packet_buffer_list;
        for(j=0; j < num_data_block + num_fec_block; ++j) {
            p->valid = 0;
            p->crc_correct = 0;
            p->len = 0;
            p++;
        }
        rb++;
    }
}

void process_payload(uint8_t *data, size_t data_len, int crc_correct, block_buffer_t *block_buffer_list, int adapter_no) {
    wifi_packet_header_t *wph;
    int block_num;
    int packet_num;
    int i;
    int kbitrate = 0;

    wph = (wifi_packet_header_t*)data;
    data += sizeof(wifi_packet_header_t);
    data_len -= sizeof(wifi_packet_header_t);

    block_num = wph->sequence_number / (num_data_block+num_fec_block);//if aram_data_packets_per_block+num_fec_block would be limited to powers of two, this could be replaced by a logical AND operation

    //debug_print("adap %d rec %x blk %x crc %d len %d\n", adapter_no, wph->sequence_number, block_num, crc_correct, data_len);

    //we have received a block number that exceeds the currently seen ones -> we need to make room for this new block
    //or we have received a block_num that is several times smaller than the current window of buffers -> this indicated that either the window is too small or that the transmitter has been restarted
    int tx_restart = (block_num + 128*param_block_buffers < max_block_num);
    if((block_num > max_block_num || tx_restart) && crc_correct) {
        if(tx_restart) {
            wbc_rx_status->tx_restart_cnt++;
            wbc_rx_status->received_block_cnt = 0;
            wbc_rx_status->damaged_block_cnt = 0;
            wbc_rx_status->received_packet_cnt = 0;
            wbc_rx_status->lost_packet_cnt = 0;
            wbc_rx_status->kbitrate = 0;
            int g;
            for(g=0; g<MAX_PENUMBRA_INTERFACES; ++g) {
                wbc_rx_status->adapter[g].received_packet_cnt = 0;
                wbc_rx_status->adapter[g].wrong_crc_cnt = 0;
                wbc_rx_status->adapter[g].current_signal_dbm = -126;
                wbc_rx_status->adapter[g].signal_good = 0;
            }
            //fprintf(stderr, "TX re-start detected\n");
            block_buffer_list_reset(block_buffer_list, param_block_buffers, num_data_block + num_fec_block);
        }

        //first, find the minimum block num in the buffers list. this will be the block that we replace
        int min_block_num = INT_MAX;
        int min_block_num_idx;
        for(i=0; i<param_block_buffers; ++i) {
            if(block_buffer_list[i].block_num < min_block_num) {
                min_block_num = block_buffer_list[i].block_num;
                min_block_num_idx = i;
            }
        }

        //debug_print("removing block %x at index %i for block %x\n", min_block_num, min_block_num_idx, block_num);

        packet_buffer_t *packet_buffer_list = block_buffer_list[min_block_num_idx].packet_buffer_list;
        int last_block_num = block_buffer_list[min_block_num_idx].block_num;

        if(last_block_num != -1) {
            wbc_rx_status->received_block_cnt++;

            //we have both pointers to the packet buffers (to get information about crc and vadility) and raw data pointers for fec_decode
            packet_buffer_t *data_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            packet_buffer_t *fec_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            int datas_missing = 0, datas_corrupt = 0, fecs_missing = 0,fecs_corrupt = 0;
            int di = 0, fi = 0;

            //first, split the received packets into DATA a FEC packets and count the damaged packets
            i = 0;
            while(di < num_data_block || fi < num_fec_block) {
                if(di < num_data_block) {
                    data_pkgs[di] = packet_buffer_list + i++;
                    data_blocks[di] = data_pkgs[di]->data;
                    if(!data_pkgs[di]->valid) datas_missing++;
                    // if(data_pkgs[di]->valid && !data_pkgs[di]->crc_correct) datas_corrupt++; // not needed as we dont receive fcs fail frames
                    di++;
                }
                if(fi < num_fec_block) {
                    fec_pkgs[fi] = packet_buffer_list + i++;
                    if(!fec_pkgs[fi]->valid) fecs_missing++;
                    // if(fec_pkgs[fi]->valid && !fec_pkgs[fi]->crc_correct) fecs_corrupt++; // not needed as we dont receive fcs fail frames
                    fi++;
                }
            }

            const int good_fecs_c = num_fec_block - fecs_missing - fecs_corrupt;
            const int datas_missing_c = datas_missing;
            const int datas_corrupt_c = datas_corrupt;
            const int fecs_missing_c = fecs_missing;
            //            const int fecs_corrupt_c = fecs_corrupt;

            int packets_lost_in_block = 0;
            //            int good_fecs = good_fecs_c;
            //the following three fields are infos for fec_decode
            unsigned int fec_block_nos[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            unsigned int erased_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            unsigned int nr_fec_blocks = 0;

            if(datas_missing_c + fecs_missing_c > 0) {
                packets_lost_in_block = (datas_missing_c + fecs_missing_c);
                wbc_rx_status->lost_packet_cnt = wbc_rx_status->lost_packet_cnt + packets_lost_in_block;
            }

            wbc_rx_status->received_packet_cnt = wbc_rx_status->received_packet_cnt + num_data_block + num_fec_block - packets_lost_in_block;

            packets_missing_last = packets_missing;
            packets_missing = packets_lost_in_block;

            if (packets_missing < packets_missing_last) { // if we have less missing packets than last time, ignore
                packets_missing = packets_missing_last;
            }

            pm_now = current_timestamp();
            if (pm_now - pm_prev_time > 220) {
                pm_prev_time = current_timestamp();
                //fprintf(stderr, "miss: %d   last: %d\n", packets_missing,packets_missing_last);
                wbc_rx_status->lost_per_block_cnt = packets_missing;
                packets_missing = 0;
                packets_missing_last = 0;
            }

            fi = 0;
            di = 0;

            //look for missing DATA and replace them with good FECs
            while(di < num_data_block && fi < num_fec_block) {
                //if this data is fine we go to the next
                if(data_pkgs[di]->valid && data_pkgs[di]->crc_correct) { di++; continue; }
                //if this DATA is corrupt and there are less good fecs than missing datas we cannot do anything for this data
                //if(data_pkgs[di]->valid && !data_pkgs[di]->crc_correct && good_fecs <= datas_missing) { di++; continue; } // not needed as we dont receive fcs fail frames
                //if this FEC is not received we go on to the next
                if(!fec_pkgs[fi]->valid) { fi++; continue; }
                //if this FEC is corrupted and there are more lost packages than good fecs we should replace this DATA even with this corrupted FEC // not needed as we dont receive fcs fail frames
                //if(!fec_pkgs[fi]->crc_correct && datas_missing > good_fecs) { fi++; continue; }

                if(!data_pkgs[di]->valid) datas_missing--;
                //else if(!data_pkgs[di]->crc_correct) datas_corrupt--; // not needed as we dont receive fcs fail frames
                //if(fec_pkgs[fi]->crc_correct) good_fecs--; // not needed as we dont receive fcs fail frames
                //at this point, data is invalid and fec is good -> replace data with fec
                erased_blocks[nr_fec_blocks] = di;
                fec_block_nos[nr_fec_blocks] = fi;
                fec_blocks[nr_fec_blocks] = fec_pkgs[fi]->data;
                di++;
                fi++;
                nr_fec_blocks++;
            }

            int reconstruction_failed = datas_missing_c + datas_corrupt_c > good_fecs_c;
            if(reconstruction_failed) {
                //we did not have enough FEC packets to repair this block
                wbc_rx_status->damaged_block_cnt++;
                //fprintf(stderr, "Could not fully reconstruct block %x! Damage rate: %f (%d / %d blocks)\n", last_block_num, 1.0 * wbc_rx_status->damaged_block_cnt / wbc_rx_status->received_block_cnt, wbc_rx_status->damaged_block_cnt, wbc_rx_status->received_block_cnt);
                //debug_print("Data mis: %d\tData corr: %d\tFEC mis: %d\tFEC corr: %d\n", datas_missing_c, datas_corrupt_c, fecs_missing_c, fecs_corrupt_c);
            }

            //decode data and write it to STDOUT
            fec_decode((unsigned int) pack_size, data_blocks, num_data_block, fec_blocks, fec_block_nos, erased_blocks, nr_fec_blocks);
            for(i=0; i<num_data_block; ++i) {
                payload_header_t *ph = (payload_header_t*)data_blocks[i];

                if(!reconstruction_failed || data_pkgs[i]->valid) {
                    //if reconstruction did fail, the data_length value is undefined. better limit it to some sensible value
                    if(ph->data_length > pack_size) ph->data_length = pack_size;

                    write(STDOUT_FILENO, data_blocks[i] + sizeof(payload_header_t), ph->data_length);
                    fflush(stdout);
                    now = current_timestamp();
                    bytes_written = bytes_written + ph->data_length;
                    if (now - prev_time > 500) {
                        prev_time = current_timestamp();
                        kbitrate = ((bytes_written * 8) / 1024) * 2;
                        wbc_rx_status->kbitrate = kbitrate;
                        bytes_written = 0;
                    }
                }
            }

            //reset buffers
            for(i=0; i<num_data_block + num_fec_block; ++i) {
                packet_buffer_t *p = packet_buffer_list + i;
                p->valid = 0;
                p->crc_correct = 0;
                p->len = 0;
            }
        }

        block_buffer_list[min_block_num_idx].block_num = block_num;
        max_block_num = block_num;
    }

    //find the buffer into which we have to write this packet
    block_buffer_t *rbb = block_buffer_list;
    for(i=0; i<param_block_buffers; ++i) {
        if(rbb->block_num == block_num) {
            break;
        }
        rbb++;
    }

    //check if we have actually found the corresponding block. this could not be the case due to a corrupt packet
    if(i != param_block_buffers) {
        packet_buffer_t *packet_buffer_list = rbb->packet_buffer_list;
        packet_num = wph->sequence_number % (num_data_block+num_fec_block); //if retr_block_size would be limited to powers of two, this could be replace by a locical and operation

        //only overwrite packets where the checksum is not yet correct. otherwise the packets are already received correctly
        if(packet_buffer_list[packet_num].crc_correct == 0) {
            memcpy(packet_buffer_list[packet_num].data, data, data_len);
            packet_buffer_list[packet_num].len = data_len;
            packet_buffer_list[packet_num].valid = 1;
            packet_buffer_list[packet_num].crc_correct = crc_correct;
        }
    }

}

void process_packet(uint8_t *packet_data, block_buffer_t *block_buffer_list, int adapter_no) {
    PENUMBRA_RADIOTAP_DATA prd;
    uint8_t payloadBuffer[MAX_PACKET_LENGTH];
    uint8_t *payload_bytes = payloadBuffer;
    int payload_length;
    int n;
    int radiotap_header_length;
    // fetch radiotap header length from radiotap header (seems to be 36 for Atheros and 18 for Ralink)
    radiotap_header_length = (packet_data[2] + (packet_data[3] << 8));

    while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0) {
        switch (rti.this_arg_index) {
            case IEEE80211_RADIOTAP_FLAGS:
                prd.m_nRadiotapFlags = *rti.this_arg;
                break;
            case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
                dbm_last[adapter_no] = dbm[adapter_no];
                dbm[adapter_no] = (int8_t)(*rti.this_arg);
                if (dbm[adapter_no] > dbm_last[adapter_no]) {
                    dbm[adapter_no] = dbm_last[adapter_no];
                }
                break;
        }
    }
    // Ralink and Atheros both always supply the FCS to userspace, no need to check
    if (prd.m_nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS)
        payload_length -= 4;
    // TODO: disable checksum handling in process_payload(), not needed since we have fscfail disabled
    int checksum_correct = 1;
    wbc_rx_status->adapter[adapter_no].received_packet_cnt++;
    process_payload(payload_bytes, payload_length, checksum_correct, block_buffer_list, adapter_no);
}

void publish_data(uint8_t *data, int message_length){
    if (udp_enabled){
        client_video_addr.sin_addr.s_addr = inet_addr(get_ip_from_ipchecker(shID));
        sendto (udp_socket, data, message_length, 0, (struct sockaddr *) &client_video_addr,
                sizeof (client_video_addr));
    }
    // TODO: write to stdout
    // TODO: setup a TCP server and send to connected clients
}

int main(int argc, char *argv[]) {
    setpriority(PRIO_PROCESS, 0, -10);
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    process_command_line_args(argc, argv);

    int receive_sockets[DB_MAX_ADAPTERS] = {0};
    for(int i = 0; i < num_interfaces; i++)
        receive_sockets[i] = open_receive_socket(adapters[i], 'm', comm_id, DB_DIREC_GROUND, DB_PORT_VIDEO);

    shID = init_shared_memory_ip();
    wbc_rx_status = wbc_status_memory_open();
    if (ieee80211_radiotap_iterator_init(&rti,(struct ieee80211_radiotap_header *)pu8Payload, ppcapPacketHeader->len) < 0) {
        fprintf(stderr, "rx ERROR: radiotap_iterator_init < 0\n");
        return -1;
    }
    fec_init();
    block_buffer_t *block_buffer_list;
    //block buffers contain both the block_num as well as packet buffers for a block.
    block_buffer_list = malloc(sizeof(block_buffer_t) * param_block_buffers);
    for(int i=0; i<param_block_buffers; ++i)
    {
        block_buffer_list[i].block_num = -1;
        block_buffer_list[i].packet_buffer_list = lib_alloc_packet_buffer_list(num_data_block+num_fec_block, MAX_PACKET_LENGTH);
    }

    while(keeprunning) {
        /*packetcounter_ts_now[i] = current_timestamp();
          if (packetcounter_ts_now[i] - packetcounter_ts_prev[i] > 220) {
              packetcounter_ts_prev[i] = current_timestamp();
              for(i=0; i<num_interfaces; ++i) {
            packetcounter_last[i] = packetcounter[i];
            packetcounter[i] = wbc_rx_status->adapter[i].received_packet_cnt;
            if (packetcounter[i] == packetcounter_last[i]) {
                wbc_rx_status->adapter[i].signal_good = 0;
            } else {
                wbc_rx_status->adapter[i].signal_good = 1;
            }
              }
          } */
        fd_set readset;
        int max_sd = 0;
        struct timeval select_timeout;
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 1e5; // 100ms
        FD_ZERO(&readset);

        for(int i=0; i<num_interfaces; ++i){
            FD_SET(receive_sockets[i], &readset);
            if (receive_sockets[i] > max_sd) max_sd = receive_sockets[i];
        }

        int select_return = select(max_sd+1, &readset, NULL, NULL, &select_timeout);
        if(select_return == 0) continue;

        for(int i=0; i<num_interfaces; ++i) {
            if(FD_ISSET(receive_sockets[i], &readset)){
                ssize_t l = recv(receive_sockets[i], lr_buffer, DATA_UNI_LENGTH, 0); int err = errno;
                if (l > 0){
                    int radiotap_length = lr_buffer[2] | (lr_buffer[3] << 8);
                    int message_length = lr_buffer[radiotap_length+7] | (lr_buffer[radiotap_length+8] << 8); // DB_v2
                    if (pass_through){
                        // Do not decode using FEC - pure pass through
                        // TODO: Implement custom protocol in case of pass_through that tells the receiver about the adapter that it was received on
                        memcpy(data_buffer, lr_buffer+(radiotap_length + DB_RAW_V2_HEADER_LENGTH), message_length);
                        publish_data(data_buffer, message_length)
                    } else {
                        process_packet(lr_buffer, block_buffer_list, i);
                    }
                } else {
                    printf(RED "DB_VIDEO_GND: Received an error: %s\n" RESET, strerror(err));
                }
            }
        }
    }
    for(int i=0; i<num_interfaces; ++i){
        close(receive_sockets[i]);
    }
    if(udp_enabled) close(udp_socket);
    return 0;
}
