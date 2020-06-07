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

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/filter.h> // BPF
#include <linux/if_packet.h>
#include <fcntl.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "db_protocol.h"
#include "radiotap/radiotap_iter.h"

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
int expected_seq_num;

/**
 * Set a BPF filter on the socket (DroneBridge raw protocol v2)
 *
 * @param newsocket The socket file descriptor on which the BPF filter should be set
 * @param new_comm_id The communication ID that we filter for
 * @param direction Packets with what kind of directions (DB_DIREC_DRONE or DB_V2_DIREC_GROUND) are allowed to pass the filter
 * @param port The port of the module using this function. See db_protocol.h (DB_PORT_CONTROLLER, DB_PORT_COMM, ...)
 * @return The socket with set BPF filter
 */
int setBPF(int newsocket, const uint8_t new_comm_id, uint8_t direction, uint8_t port) {
    struct sock_filter dest_filter[] =
            {
                    {0x30, 0, 0, 0x00000003},
                    {0x64, 0, 0, 0x00000008},
                    {0x07, 0, 0, 0000000000},
                    {0x30, 0, 0, 0x00000002},
                    {0x4c, 0, 0, 0000000000},
                    {0x02, 0, 0, 0000000000},
                    {0x07, 0, 0, 0000000000},
                    {0x48, 0, 0, 0000000000},
                    {0x45, 1, 0, 0x0000b400},   // allow rts frames
                    {0x45, 0, 5, 0x00000800},   // allow data frames
                    {0x48, 0, 0, 0x00000004},
                    {0x15, 0, 3, 0x00000301},   // <direction><comm id>
                    {0x50, 0, 0, 0x00000006},
                    {0x15, 0, 1, 0x00000005},   // <port>
                    {0x06, 0, 0, 0x00002000},
                    {0x06, 0, 0, 0000000000},
            };

    // override some of the filter settings
    dest_filter[11].k = (uint32_t) ((0x00 << 24) | (0x00 << 16) | (direction << 8) | new_comm_id);
    dest_filter[13].k = (uint32_t) port;

    struct sock_fprog bpf =
            {
                    .len = ARRAY_SIZE(dest_filter),
                    .filter = dest_filter,
            };
    int ret = setsockopt(newsocket, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf));
    if (ret < 0) {
        perror("DB_RECEIVE: could not attach BPF ");
        close(newsocket);
        return -1;
    }
    return newsocket;
}

/**
 * Bind the socket to a network interface
 *
 * @param newsocket The socket to be bound
 * @param the_mode The DroneBridge mode we are in (monitor or wifi (unsupported))
 * @param new_ifname The name of the interface we want the socket to bind to
 * @return The socket that is bound to the network interface
 */
int bindsocket(int newsocket, char the_mode, char new_ifname[IFNAMSIZ]) {
    struct sockaddr_ll sll;
    struct ifreq ifr;
    bzero(&sll, sizeof(sll));
    bzero(&ifr, sizeof(ifr));
    strncpy((char *) ifr.ifr_name, new_ifname, IFNAMSIZ - 1);
    bzero(&sll, sizeof(sll));
    if ((ioctl(newsocket, SIOCGIFINDEX, &ifr)) == -1) {
        perror("DB_RECEIVE: Unable to find interface index ");
        return -1;
    }

    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    if (the_mode == 'w') {
        sll.sll_protocol = htons(ETHER_TYPE);
    } else {
        sll.sll_protocol = htons(ETH_P_802_2);
    }
    if ((bind(newsocket, (struct sockaddr *) &sll, sizeof(sll))) == -1) {
        perror("DB_RECEIVE: bind ");
        return -1;
    }
    return newsocket;
}

/**
 *
 * @param the_socket The socket to be set to non-blocking
 * @return The socket that is set to non-blocking
 */
void set_socket_nonblocking(int *the_socketfd) {
    if (fcntl(*the_socketfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("Can not put socket in non-blocking mode");
    }
}

/**
 *
 * @param the_socketfd The socket to be set with a timeout
 * @param time_out_s Timeout seconds
 * @param time_out_us Timeout micro seconds
 * @return The socket with a set timeout
 */
int set_socket_timeout(int the_socketfd, int time_out_s, int time_out_us) {
    struct timeval tv_timeout;
    tv_timeout.tv_sec = time_out_s;
    tv_timeout.tv_usec = time_out_us;
    setsockopt(the_socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv_timeout, sizeof(struct timeval));
    return the_socketfd;
}

/**
 * Counts the lost packets. Protocol fails if we lost more than 255 packets at once
 *
 * @param last_seq_num The sequence number of the previous packet we received
 * @param received_seq_num The sequence number of the packet we received "right now"
 * @return The number of packets we lost in between the previous packet and the most current one
 */
uint8_t count_lost_packets(uint8_t last_seq_num, uint8_t received_seq_num) {
    expected_seq_num = ((last_seq_num == 255) ? 0 : (last_seq_num + 1));
    // Could be one statement. Less confusing this way.
    if (expected_seq_num == received_seq_num) return 0;
    return (uint8_t) ((received_seq_num > expected_seq_num) ? (received_seq_num - expected_seq_num) :
                      (255 - expected_seq_num) + received_seq_num);
}

/**
 * Gets the payload from a received packet buffer of a raw socket (DB raw socket)
 *
 * @param receive_buffer: The buffer filled by the raw socket during recv()
 * @param receive_length: The length of the received raw packet (return value of recv())
 * @param payload_buffer: The buffer we write the DroneBridge payload into.
 * @param seq_num: A pointer to the variable where we write the sequence number of the packet into
 * @param radiotap_length: A pointer to the variable where we write the radiotap header length into
 */
uint16_t get_db_payload(uint8_t *receive_buffer, ssize_t receive_length, uint8_t *payload_buffer, uint8_t *seq_num,
                        uint16_t *radiotap_length) {
    *radiotap_length = receive_buffer[2] | (receive_buffer[3] << 8);
    *seq_num = receive_buffer[*radiotap_length + 9];
    uint16_t payload_length =
            receive_buffer[*radiotap_length + 7] | (receive_buffer[*radiotap_length + 8] << 8); // DB_v2
    // estimate if the packet was sent with offset payload. 4 FCS bytes may or may not be supplied at end of frame.
    if ((receive_length - *radiotap_length - DB_RAW_V2_HEADER_LENGTH) <= (payload_length + 4))
        memcpy(payload_buffer, &receive_buffer[*radiotap_length + DB_RAW_V2_HEADER_LENGTH], payload_length);
    else if (payload_length <= DATA_UNI_LENGTH)
        memcpy(payload_buffer, &receive_buffer[*radiotap_length + DB_RAW_V2_HEADER_LENGTH + DB_RAW_OFFSET],
               payload_length);
    return payload_length;
}

/**
 * Extract RSSI value from radiotap header
 * 
 * @param payload_buffer Buffer containing the received packet data including radiotap header
 * @param radiotap_length Length of radiotap header
 * @return RSSI of received packet
 */
int8_t get_rssi(uint8_t *payload_buffer, int radiotap_length) {
    struct ieee80211_radiotap_iterator rti;
    if (ieee80211_radiotap_iterator_init(&rti, (struct ieee80211_radiotap_header *) payload_buffer, radiotap_length,
                                         NULL) < 0)
        return 0;
    while ((ieee80211_radiotap_iterator_next(&rti)) == 0) {
        switch (rti.this_arg_index) {
            case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
                return (int8_t) (*rti.this_arg);
            default:
                break;
        }
    }
    return 0;
}