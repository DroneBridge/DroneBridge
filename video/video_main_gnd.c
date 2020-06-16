/*   (c) 2015 befinitiv
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

/**
 * Program to receive a continuous stream of data from the UAV unit. Stream is protected by FEC. This program decodes
 * the FEC data and outputs the payload to various end-points. Endpoints are UDP (192.192.2.1) and a UNIX domain socket
 * on /tmp/db_video_out (see db_protocol.h). The UDP destination address can be changed by sending a UDP packet of any
 * content to this application on port 5000. The source address of that packet will be the new destination address.
 * It is called a video destination hint packet.
 */

#include <stdbool.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <zconf.h>
#include <memory.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "fec.h"
#include "video_lib.h"
#include "../common/shared_memory.h"
#include "../common/db_raw_receive.h"
#include "../common/radiotap/radiotap_iter.h"
#include "../common/db_raw_send_receive.h"
#include "../common/db_common.h"
#include "../common/db_unix.h"

#define MAX_PACKET_LENGTH 4192
#define MAX_USER_PACKET_LENGTH 1450
#define MAX_DATA_OR_FEC_PACKETS_PER_BLOCK 32
#define DEBUG 0
#define UDP_BUFF_SIZE 2048

int num_interfaces = 0;
int dest_port_video, unix_sock;
uint8_t comm_id, num_data_block, num_fec_block;
uint8_t lr_buffer[MAX_DB_DATA_LENGTH] = {0};
bool pass_through, udp_enabled = true, output_to_usb_bridge = false, send_to_std_out = true;
volatile bool keeprunning = true;
int param_block_buffers = 1;
int pack_size = MAX_USER_PACKET_LENGTH;
db_gnd_status_t *db_gnd_status = NULL;
int max_block_num = -1, udp_socket;
struct sockaddr_in client_video_addr;
struct sockaddr_un unix_socket_addr;
long long prev_time = 0;
long long now = 0;
int bytes_written = 0;
socklen_t server_length = sizeof(struct sockaddr_un);

char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];
char overwrite_ip[INET6_ADDRSTRLEN];
bool fixed_ip = false;

typedef struct {
    int selectable_fd;
    int n80211HeaderLength;
} monitor_interface_t;


void int_handler(int dummy) {
    keeprunning = false;
}

long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    return milliseconds;
}


/**
 * Init UDP socket bound to port 5000 for sending UDP video stream & for receiving video destination hints
 */
void init_outputs() {
    if (pass_through) dest_port_video = APP_PORT_VIDEO_FEC;
    if (udp_enabled) {
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        client_video_addr.sin_family = AF_INET;
        client_video_addr.sin_addr.s_addr = inet_addr(DB_AP_CLIENT_IP);
        client_video_addr.sin_port = htons(dest_port_video);
        int optval = 1;
        setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));

        struct sockaddr_in udp_server_addr;
        bzero((char *) &udp_server_addr, sizeof(udp_server_addr));
        udp_server_addr.sin_family = AF_INET;
        udp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        udp_server_addr.sin_port = htons(APP_PORT_VIDEO);
        if ((bind(udp_socket, (struct sockaddr *) &udp_server_addr, sizeof(udp_server_addr))) != 0) {
            perror("DB_VIDEO_GND: UDP socket bind failed ");
        }
    }
}

/**
 * Write final data to various outputs (UDP, (TCP) etc.)
 *
 * @param data Data to publish
 * @param message_length Lenght of data
 * @param fec_decoded Indicator if the data also contains FEC packets. True if pure DATA packets (and fully decoded FEC)
 */
void publish_data(uint8_t *data, uint32_t message_length, bool fec_decoded) {
    if (output_to_usb_bridge) {
        // We assume the consumer is faster than the producer and that it will always be able to send
        if (sendto(unix_sock, data, message_length, 0, (struct sockaddr *) &unix_socket_addr, server_length) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // ignore non existing dst socket addr. usbbridge might not have a connected dev
                if (errno != ENOENT && errno != ECONNREFUSED)
                    perror("DB_VIDEO_GND: Error sending via UNIX domain socket");
                // else: usbbridge might not started or device not connected
            } // else
//                LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Error sending to unix domain - might lost a packet\n");
        }
    }
    if (udp_enabled) {
        if (sendto(udp_socket, data, message_length, 0, (struct sockaddr *) &client_video_addr,
                   sizeof(client_video_addr)) < message_length)
            LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Not all data sent via UDP (msg size %ui) > %s\n", message_length, strerror(errno));
    }
    if (send_to_std_out && fec_decoded) {
        // only output decoded fec packets to stdout so that video player can read data stream directly
        if (write(STDOUT_FILENO, data, message_length) < 0)
            LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Error writing to stdout %s\n", strerror(errno));
    }
    now = current_timestamp();
    bytes_written += message_length;
    if (now - prev_time > 500) {
        prev_time = current_timestamp();
        uint32_t kbitrate = (uint32_t) (((bytes_written * 8) / 1024) * 2);
        db_gnd_status->kbitrate = kbitrate;
        bytes_written = 0;
    }
}

void block_buffer_list_reset(block_buffer_t *block_buffer_list, int block_buffer_list_len) {
    int i;
    block_buffer_t *rb = block_buffer_list;

    for (i = 0; i < block_buffer_list_len; ++i) {
        rb->block_num = -1;

        int j;
        packet_buffer_t *p = rb->packet_buffer_list;
        for (j = 0; j < num_data_block + num_fec_block; ++j) {
            p->valid = 0;
            p->crc_correct = 0;
            p->len = 0;
            p++;
        }

        rb++;
    }
}

/**
 * Takes a stream of payload (FEC & DATA) and does error correction publishing the corrected data in the end
 *
 * @param data: The payload of raw protocol (a db_video_packet_t)
 * @param data_len: Length of the payload
 * @param crc_correct: Was the FCF of the raw packet OK
 * @param block_buffer_list: An array of block_buffer_t structs
 */
void process_video_payload(uint8_t *data, uint16_t data_len, int crc_correct, block_buffer_t *block_buffer_list) {
    uint block_num;
    uint packet_num;
    int i;

    db_video_packet_t *db_video_packet = (db_video_packet_t *) data;
    //if aram_data_packets_per_block+num_fec_block would be limited to powers of two, this could be replaced by a logical AND operation
    block_num = db_video_packet->video_packet_header.sequence_number / (num_data_block + num_fec_block);

    //LOG_SYS_STD(LOG_ERR, "seq %i blk %i crc %d len %i\n", db_video_packet->video_packet_header.sequence_number, block_num, crc_correct, (int) data_len);

    //we have received a block number that exceeds the currently seen ones -> we need to make room for this new block
    //or we have received a block_num that is several times smaller than the current window of buffers -> this indicated that either the window is too small or that the transmitter has been restarted
    int tx_restart = (block_num + 128 * param_block_buffers < max_block_num);
    // with block_buffer_list length == 1 (d=1) this means we received all packets of a block (we still might miss some)
    if ((block_num > max_block_num || tx_restart) && crc_correct) {
        if (tx_restart) {
            db_gnd_status->tx_restart_cnt++;
            LOG_SYS_STD(LOG_ERR,
                        "TX RESTART: Detected blk %x that lies outside of the current retr block buffer window "
                        "(max_block_num = %x) (if there was no tx restart, increase window size via -d)\n",
                        block_num, max_block_num);
            block_buffer_list_reset(block_buffer_list, param_block_buffers);
        }
        //first, find the minimum block num in the buffers list. this will be the block that we replace
        int min_block_num = INT_MAX;
        int min_block_num_idx = 0;
        for (i = 0; i < param_block_buffers; ++i) {
            if (block_buffer_list[i].block_num < min_block_num) {
                min_block_num = block_buffer_list[i].block_num;
                min_block_num_idx = i;
            }
        }

        //debug_print("removing block %x at index %i for block %x\n", min_block_num, min_block_num_idx, block_num);

        packet_buffer_t *packet_buffer_list = block_buffer_list[min_block_num_idx].packet_buffer_list;
        int last_block_num = block_buffer_list[min_block_num_idx].block_num;

        if (last_block_num != -1) {
            //db_gnd_status->adapter[].received_block_cnt++;

            //we have both pointers to the packet buffers (to get information about crc and vadility) and raw data pointers for fec_decode
            packet_buffer_t *data_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            packet_buffer_t *fec_pkgs[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            int datas_missing = 0, datas_corrupt = 0, fecs_missing = 0, fecs_corrupt = 0;
            uint di = 0, fi = 0;


            // first, split the received packets into DATA a FEC packets and count the damaged packets
            // We assume that the packets are correctly ordered inside the packet buffer list
            i = 0;
            while (di < num_data_block || fi < num_fec_block) {
                if (di < num_data_block) {
                    data_pkgs[di] = packet_buffer_list + i++;
                    data_blocks[di] = data_pkgs[di]->data;
                    if (!data_pkgs[di]->valid)
                        datas_missing++;
                    if (data_pkgs[di]->valid && !data_pkgs[di]->crc_correct)
                        datas_corrupt++;
                    di++;
                }

                if (fi < num_fec_block) {
                    fec_pkgs[fi] = packet_buffer_list + i++;
                    if (!fec_pkgs[fi]->valid)
                        fecs_missing++;

                    if (fec_pkgs[fi]->valid && !fec_pkgs[fi]->crc_correct)
                        fecs_corrupt++;

                    fi++;
                }
            }

            const int good_fecs_c = num_fec_block - fecs_missing - fecs_corrupt;
            const int datas_missing_c = datas_missing;
            const int datas_corrupt_c = datas_corrupt;

            int good_fecs = good_fecs_c;
            //the following three fields are infos for fec_decode
            unsigned int fec_block_nos[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            unsigned int erased_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
            unsigned short nr_fec_blocks = 0;

            fi = 0;
            di = 0;

            //look for missing DATA and replace them with good FECs
            while (di < num_data_block && fi < num_fec_block) {
                //if this data is fine we go to the next
                if (data_pkgs[di]->valid && data_pkgs[di]->crc_correct) {
                    di++;
                    continue;
                }

                //if this DATA is corrupt and there are less good fecs than missing datas we cannot do anything for this data
                if (data_pkgs[di]->valid && !data_pkgs[di]->crc_correct && good_fecs <= datas_missing) {
                    di++;
                    continue;
                }

                //if this FEC is not received we go on to the next
                if (!fec_pkgs[fi]->valid) {
                    fi++;
                    continue;
                }

                //if this FEC is corrupted and there are more lost packages than good fecs we should replace this DATA even with this corrupted FEC
                if (!fec_pkgs[fi]->crc_correct && datas_missing > good_fecs) {
                    fi++;
                    continue;
                }


                if (!data_pkgs[di]->valid)
                    datas_missing--;
                else if (!data_pkgs[di]->crc_correct)
                    datas_corrupt--;

                if (fec_pkgs[fi]->crc_correct)
                    good_fecs--;

                //at this point, data is invalid and fec is good -> replace data with fec
                erased_blocks[nr_fec_blocks] = di;
                fec_block_nos[nr_fec_blocks] = fi;
                fec_blocks[nr_fec_blocks] = fec_pkgs[fi]->data;
                di++;
                fi++;
                nr_fec_blocks++;
            }

            int reconstruction_failed = datas_missing_c + datas_corrupt_c > good_fecs_c;

            if (reconstruction_failed) {
                //we did not have enough FEC packets to repair this block
                db_gnd_status->damaged_block_cnt++;
                //LOG_SYS_STD(LOG_ERR, "Could not fully reconstruct block %x! Damage rate: %f (%d / %d blocks)\n", last_block_num, 1.0 * rx_status->damaged_block_cnt / rx_status->received_block_cnt, rx_status->damaged_block_cnt, rx_status->received_block_cnt);
                //debug_print("Data mis: %d\tData corr: %d\tFEC mis: %d\tFEC corr: %d\n", datas_missing_c, datas_corrupt_c, fecs_missing_c, fecs_corrupt_c);
            }


            //decode data and publish it
            fec_decode((unsigned int) pack_size, data_blocks, num_data_block, fec_blocks, fec_block_nos, erased_blocks,
                       nr_fec_blocks);
            for (i = 0; i < num_data_block; ++i) {
                video_packet_data_t *vpd_corrected = (video_packet_data_t *) data_blocks[i];

                if (!reconstruction_failed || data_pkgs[i]->valid) {
                    //if reconstruction did fail, the data_length value is undefined. better limit it to some sensible value
                    if (vpd_corrected->data_length > pack_size) {
                        vpd_corrected->data_length = (uint32_t) pack_size;
                    }
                    // do not publish the data_length field of video_packet_data_t struct
                    publish_data(data_blocks[i] + 4, vpd_corrected->data_length - 4, true);
                }
            }


            //reset buffers
            for (i = 0; i < num_data_block + num_fec_block; ++i) {
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
    for (i = 0; i < param_block_buffers; ++i) {
        if (rbb->block_num == block_num) {
            break;
        }
        rbb++;
    }

    //check if we have actually found the corresponding block. this could not be the case due to a corrupt packet
    if (i != param_block_buffers) {
        packet_buffer_t *packet_buffer_list = rbb->packet_buffer_list;
        packet_num = db_video_packet->video_packet_header.sequence_number % (num_data_block +
                                                                             num_fec_block); //if retr_block_size would be limited to powers of two, this could be replace by a locical and operation

        //only overwrite packets where the checksum is not yet correct. otherwise the packets are already received correctly
        if (packet_buffer_list[packet_num].crc_correct == 0) {
            memcpy(packet_buffer_list[packet_num].data, data + sizeof(video_packet_header_t),
                   data_len - sizeof(video_packet_header_t));
            packet_buffer_list[packet_num].len = (uint) (data_len - sizeof(video_packet_header_t));
            packet_buffer_list[packet_num].valid = 1;
            packet_buffer_list[packet_num].crc_correct = crc_correct;
        }
    }
    // TODO: Check if we got all possible packets of a block already and decode, no need to wait for a packet of the next block to indicate
}

/**
 * Extracts the payload from received packet, reads radiotap header for RSSI info and forwards payload to decoding stage
 *
 * @param interface
 * @param block_buffer_list
 * @param adapter_no
 */
void process_packet(monitor_interface_t *interface, block_buffer_t *block_buffer_list, int adapter_no) {
    struct ieee80211_radiotap_iterator rti;

    uint8_t payload_buffer[DATA_UNI_LENGTH]; // contains payload of raw protocol (video header + data = db_video_packet)
    uint16_t radiotap_length = 0;
    int checksum_correct = 1;
    uint8_t current_antenna_indx = 0, seq_num_video = 0;
    uint16_t message_length = 0;

    // receive
    ssize_t l = recv(interface->selectable_fd, lr_buffer, MAX_DB_DATA_LENGTH, 0);
    int err = errno;
    if (l > 0) {
        db_gnd_status->received_packet_cnt++;
        message_length = get_db_payload(lr_buffer, l, payload_buffer, &seq_num_video, &radiotap_length);
        if (pass_through) {
            // Do not decode using FEC - pure UDP pass through, decoding of FEC must happen on following applications
            // TODO: Implement custom protocol in case of pass_through that tells the receiver about the adapter that it was received on
            publish_data(payload_buffer, message_length, false);
        }
        if (ieee80211_radiotap_iterator_init(&rti, (struct ieee80211_radiotap_header *) lr_buffer, radiotap_length,
                                             NULL) != 0) {
            LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Could not init radiotap header\n");
            return;
        }
        while ((ieee80211_radiotap_iterator_next(&rti)) == 0) {
            switch (rti.this_arg_index) {
                case IEEE80211_RADIOTAP_RATE:
                    db_gnd_status->adapter[adapter_no].rate = (*rti.this_arg);
                    break;
                case IEEE80211_RADIOTAP_ANTENNA:
                    current_antenna_indx = (*rti.this_arg);
                    break;
                case IEEE80211_RADIOTAP_FLAGS:
                    checksum_correct = (*rti.this_arg & IEEE80211_RADIOTAP_F_BADFCS) == 0;
                    break;
                case IEEE80211_RADIOTAP_LOCK_QUALITY:
                    db_gnd_status->adapter[adapter_no].lock_quality = (*rti.this_arg);
                case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
                    if (current_antenna_indx == 0) // first occurrence in header will be general RSSI
                        db_gnd_status->adapter[adapter_no].current_signal_dbm = (int8_t) (*rti.this_arg);
                    if (current_antenna_indx <= MAX_ANTENNA_CNT)
                        db_gnd_status->adapter[adapter_no].ant_signal_dbm[current_antenna_indx] = (int8_t) (*rti.this_arg);
                    break;
                default:
                    break;
            }
        }
        db_gnd_status->adapter[adapter_no].num_antennas = (uint8_t) (current_antenna_indx + 1);
        if (!checksum_correct)
            db_gnd_status->adapter[adapter_no].wrong_crc_cnt++;
        db_gnd_status->adapter[adapter_no].received_packet_cnt++;

        db_gnd_status->last_update = time(NULL);
        process_video_payload(payload_buffer, message_length, checksum_correct, block_buffer_list);
    } else {
        LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Received an error: %s\n", strerror(err));
    }
}

void process_command_line_args(int argc, char *argv[]) {
    num_interfaces = 0, comm_id = DEFAULT_V2_COMMID, pass_through = false, udp_enabled = true, send_to_std_out = true;
    num_data_block = 8, num_fec_block = 4, pack_size = 1024, dest_port_video = APP_PORT_VIDEO;
    int c;
    while ((c = getopt(argc, argv, "n:c:r:f:p:d:u:v:i:os")) != -1) {
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
                pack_size = (int) strtol(optarg, NULL, 10);
                break;
            case 'p':
                if (*optarg == 'Y')
                    pass_through = true; // encoded FEC packets pass through via UDP
                break;
            case 'u':
                if (*optarg == 'N')
                    udp_enabled = false;
                else
                    udp_enabled = true;
                break;
            case 'v':
                dest_port_video = (int) strtol(optarg, NULL, 10);
                break;
            case 'i':
                fixed_ip = true;
                strncpy(overwrite_ip, optarg, INET6_ADDRSTRLEN);
                break;
            case 'o':
                output_to_usb_bridge = true;
                break;
            case 's':
                send_to_std_out = false;
                break;
            default:
                printf("Based of Wifibroadcast by befinitiv, based on packet spammer by Andy Green.  Licensed under GPL2\n"
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
                       "\n\t-u <Y|N> to enable or disable UDP forwarding of decoded data"
                       "\n\t-i UDP DST IP overwrite: Ignore DroneBridge IP checker shared memory and send data to this IP"
                       "\n\t-p <Y|N> to enable/disable pass through of encoded FEC packets via UDP to port: %i"
                       "\n\t-v Destination port of video stream when set via UDP (IP checker address) or TCP"
                       "\n\t-o Send to output to unix domain socket at %s so that DroneBridge USBBridge can forward it"
                       "\n\t-s Disable decoded output to stdout",
                       1024, MAX_USER_PACKET_LENGTH, APP_PORT_VIDEO_FEC, DB_UNIX_DOMAIN_VIDEO_PATH);
                abort();
        }
    }
}

// Log to LOG_NOTICE since it is directed to stderr via LOG_SYS_STD.
// Do not log to stdout! It is for video data only!
int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    setpriority(PRIO_PROCESS, 0, -10);
    monitor_interface_t interfaces[MAX_PENUMBRA_INTERFACES];
    int i;
    struct sockaddr_in udp_video_hint_src;
    uint8_t udp_buff[UDP_BUFF_SIZE];
    block_buffer_t *block_buffer_list;

    process_command_line_args(argc, argv);
    if (num_interfaces == 0) {
        LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: No interface specified. Aborting\n");
        abort();
    }

    if (pack_size > MAX_USER_PACKET_LENGTH) {
        LOG_SYS_STD(LOG_ERR, "Packet length is limited to %d bytes (you requested %d bytes)\n", MAX_USER_PACKET_LENGTH,
                    pack_size);
        abort();
    }

    fec_init();
    init_outputs();
    if (fixed_ip && udp_enabled) {
        LOG_SYS_STD(LOG_NOTICE, "DB_VIDEO_GND: Sending to %s\n", overwrite_ip);
        client_video_addr.sin_addr.s_addr = inet_addr(overwrite_ip);
    }

    db_gnd_status = db_gnd_status_memory_open();
    db_gnd_status->wifi_adapter_cnt = (uint32_t) num_interfaces;
    db_gnd_status->received_packet_cnt = 0;
    db_gnd_status->received_block_cnt = 0;
    db_gnd_status->damaged_block_cnt = 0;
    db_gnd_status->tx_restart_cnt = 0;

    // init DroneBridge raw sockets to listen for incoming data
    for (int j = 0; j < num_interfaces; ++j) {
        db_socket_t db_sock = open_db_socket(adapters[j], comm_id, 'm', 11, DB_DIREC_DRONE, DB_PORT_VIDEO, DB_FRAMETYPE_DATA);
        interfaces[j].selectable_fd = db_sock.db_socket;
        strcpy(db_gnd_status->adapter[j].name, adapters[j]);
        LOG_SYS_STD(LOG_NOTICE, "\t%s\n", db_gnd_status->adapter[j].name);
        db_gnd_status->adapter[j].received_packet_cnt = 0;
        db_gnd_status->adapter[j].wrong_crc_cnt = 0;
        db_gnd_status->adapter[j].current_signal_dbm = -100;
    }
    // init UNIX domain master socket to which local clients can connect & get video data in an UDP like fashion
    unix_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_sock < 0) {
        LOG_SYS_STD(LOG_ERR, "DB_VIDEO_GND: Failed opening UNIX domain socket\n");
        exit(-1);
    }
    set_socket_nonblocking(&unix_sock);
    memset(&unix_socket_addr, 0x00, sizeof(unix_socket_addr));
    unix_socket_addr.sun_family = AF_UNIX;
    strcpy(unix_socket_addr.sun_path, DB_UNIX_DOMAIN_VIDEO_PATH);
    // UDP server socket to receive video dst hints

    //block buffers contain both the block_num as well as packet buffers for a block.
    block_buffer_list = malloc(sizeof(block_buffer_t) * param_block_buffers);
    for (i = 0; i < param_block_buffers; ++i) {
        block_buffer_list[i].block_num = -1;
        block_buffer_list[i].packet_buffer_list = lib_alloc_packet_buffer_list(num_data_block + num_fec_block,
                                                                               MAX_PACKET_LENGTH);
    }

    LOG_SYS_STD(LOG_NOTICE, "DB_VIDEO_GND: started on %i interfaces\n", num_interfaces);
    fd_set readset;
    unsigned int client_address_size = sizeof(udp_video_hint_src);
    while (keeprunning) {
        FD_ZERO(&readset);

        int max_sd = udp_socket;
        FD_SET(udp_socket, &readset);
        for (i = 0; i < num_interfaces; i++) {
            FD_SET(interfaces[i].selectable_fd, &readset);
            if (interfaces[i].selectable_fd > max_sd)
                max_sd = interfaces[i].selectable_fd;
        }

        int select_return = select(max_sd + 1, &readset, NULL, NULL, NULL);
        if (select_return == -1 && errno != EINTR) {
            perror("DB_VIDEO_GND: select() returned error: ");
        } else if (select_return > 0) {
            if (FD_ISSET(udp_socket, &readset)) {
                // received a video destination hint. Update video destination udp address
                if (recvfrom(udp_socket, udp_buff, UDP_BUFF_SIZE, 0, (struct sockaddr *) &udp_video_hint_src,
                             &client_address_size) != -1) {
                    client_video_addr.sin_addr.s_addr = udp_video_hint_src.sin_addr.s_addr;
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(client_video_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                    LOG_SYS_STD(LOG_NOTICE, "Changed destination IP to %s\n", ip_str);
                } else
                    perror("DB_VIDEO_GND: Error receiving on UDP socket: ");
            }
            for (i = 0; i < num_interfaces; i++) {
                if (FD_ISSET(interfaces[i].selectable_fd, &readset)) {
                    process_packet(&interfaces[i], block_buffer_list, i);
                }
            }
        }
    }

    for (int g = 0; i < num_interfaces; ++i) {
        close(interfaces[g].selectable_fd);
    }
    unlink(DB_UNIX_DOMAIN_VIDEO_PATH);
    close(unix_sock);
    if (udp_enabled) close(udp_socket);
    LOG_SYS_STD(LOG_NOTICE, "DB_VIDEO_GND: Terminated\n");
    return (0);
}
