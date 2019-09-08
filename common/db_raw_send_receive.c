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

#include <sys/socket.h>
#include <stdint.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/if_ether.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>
#include <linux/if_packet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "db_protocol.h"
#include "db_raw_send_receive.h"
#include "db_raw_receive.h"
#include "db_common.h"
#include "db_utils.h"

uint8_t radiotap_header_pre[] = {
        0x00, 0x00, // <-- radiotap version
        0x0d, 0x00, // <- radiotap header length
        0x04, 0x80, 0x00, 0x00, // <-- bitmap
        0x18,       // data rate (will be overwritten)
        0x00,
        0x00, 0x00, 0x00
};

const uint8_t frame_control_pre_rts[] =
        {
                0xb4, 0x00, 0x00, 0x00
        };
const uint8_t frame_control_pre_data[] =
        {
                0x08, 0x00, 0x00, 0x00
        };
// Not implemented with the BPF filter. Sending possible but DB socket will not receive it
const uint8_t frame_control_pre_beacon[] =
        {
                0x80, 0x00, 0x00, 0x00
        };

struct ifreq raw_if_idx;
struct ifreq raw_if_mac;
char interfaceName[IFNAMSIZ];
struct sockaddr_ll socket_address;
char mode = 'm';
int db_raw_offset = 0; // offset between payload and DB raw header. Needed when drivers overwrite payload with 802.11 SQN

// init packet structure
uint8_t monitor_framebuffer[RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DATA_UNI_LENGTH] = {0};
struct radiotap_header *rth = (struct radiotap_header *) monitor_framebuffer;
struct db_raw_v2_header_t *db_raw_header = (struct db_raw_v2_header_t *) (monitor_framebuffer + RADIOTAP_LENGTH);

/**
 * Set the transmission bit rate in the radiotap header. Only works with ralink cards.
 * Can be used to change the bitrate before every transmission.
 * 
 * @param bitrate_option Bit rate in Mbps
 */
void set_bitrate(int bitrate_option) {
    switch (bitrate_option) {
        case 1:
            rth->bytes[8] = 0x02;
            break;
        case 2:
            rth->bytes[8] = 0x04;
            break;
        case 5: // 5.5
            rth->bytes[8] = 0x14;
            break;
        case 6:
            rth->bytes[8] = 0x0C;
            break;
        case 9:
            rth->bytes[8] = 0x12;
            break;
        case 11:
            rth->bytes[8] = 0x16;
            break;
        case 12:
            rth->bytes[8] = 0x18;
            break;
        case 18:
            rth->bytes[8] = 0x24;
            break;
        case 24:
            rth->bytes[8] = 0x30;
            break;
        case 36:
            rth->bytes[8] = 0x48;
            break;
        case 48:
            rth->bytes[8] = 0x60;
            break;
        case 54:
            rth->bytes[8] = 0x6C;
            break;
        default:
            rth->bytes[8] = 0x0c;
            LOG_SYS_STD(LOG_ERR, "DroneBridgeCommon: Wrong bitrate option. Setting to 6Mbps\n");
    }
}


/**
 * Setup of the the DroneBridge raw protocol v2 header
 * 
 * @param comm_id
 * @param bitrate_option
 * @param frame_type
 * @param send_direction
 * @param frame_type The type of raw frame being sent: 1=RTS, 2=DATA
 * @return The socket file descriptor in case of a success or -1 if we screwed up
 */
int conf_monitor(int sockfd, uint8_t comm_id, int bitrate_option, uint8_t send_direction, uint8_t new_port,
                 uint8_t frame_type) {
    memset(monitor_framebuffer, 0, (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DATA_UNI_LENGTH));
    memcpy(rth->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    set_bitrate(bitrate_option);
    // build custom DroneBridge v2 header
    switch (frame_type) {
        case DB_FRAMETYPE_RTS:
            memcpy(db_raw_header->fcf_duration, frame_control_pre_rts, 4);
            break;
        case DB_FRAMETYPE_BEACON:
            LOG_SYS_STD(LOG_NOTICE, "Warning! Receiving of beacon frames with DB sockets not supported. Test only!\n");
            memcpy(db_raw_header->fcf_duration, frame_control_pre_beacon, 4);
            break;
        case DB_FRAMETYPE_DATA:
        default:
            memcpy(db_raw_header->fcf_duration, frame_control_pre_data, 4);
            break;
    }
    db_raw_header->direction = send_direction;
    db_raw_header->comm_id = comm_id;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, IFNAMSIZ) < 0) {
        LOG_SYS_STD(LOG_ERR,
                    "DroneBridgeCommon: Error binding monitor socket to interface. Closing socket. Please restart.\n");
        close(sockfd);
        return -1;
    }
    /* Index of the network device */
    socket_address.sll_ifindex = raw_if_idx.ifr_ifindex;
    uint8_t recv_direction = (uint8_t) ((send_direction == DB_DIREC_DRONE) ? DB_DIREC_GROUND : DB_DIREC_DRONE);
    sockfd = setBPF(sockfd, comm_id, recv_direction, new_port);
    clear_socket_buffer(sockfd);
    return sockfd;
}


/**
 * Opens and configures a socket for sending and receiving DroneBridge raw protocol frames
 * 
 * @param ifName Name of the network interface the socket is bound to
 * @param comm_id The communication ID
 * @param trans_mode The transmission mode (m|w) for monitor or wifi
 * @param bitrate_option Transmission bit rate. Only works with Ralink cards
 * @param send_direction Are the sent packets for the drone or the ground station
 * @param receive_new_port Port the BPF filter gets set to. Port open for receiving data.
 * @param frame_type The type of raw frame being sent: 1=RTS, 2=DATA
 * @return the socket file descriptor or -1 if something went wrong
 */
db_socket_t open_db_socket(char *ifName, uint8_t comm_id, char trans_mode, int bitrate_option,
                           uint8_t send_direction, uint8_t receive_new_port, uint8_t frame_type) {
    mode = trans_mode;
    db_socket_t new_socket;
    int socket_fd;
    if (mode == 'w') {
        // TODO: ignore for now. I will be UDP in future.
        if ((socket_fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("Error opening raw interface for WiFi mode ");
            new_socket.db_socket = -1;
            return new_socket;
        }
    } else {
        if ((socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
            perror("Error opening raw interface ");
            new_socket.db_socket = -1;
            return new_socket;
        }
    }

    /* Get the index of the interface to send on */
    memset(&raw_if_idx, 0, sizeof(struct ifreq));
    strncpy(raw_if_idx.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_fd, SIOCGIFINDEX, &raw_if_idx) < 0) {
        perror("DroneBridgeCommon: SIOCGIFINDEX");
        new_socket.db_socket = -1;
        LOG_SYS_STD(LOG_ERR, "%s", raw_if_idx.ifr_name);
        return new_socket;
    }
    /* Get the MAC address of the interface to send on */
    memset(&raw_if_mac, 0, sizeof(struct ifreq));
    strncpy(raw_if_mac.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_fd, SIOCGIFHWADDR, &raw_if_mac) < 0) {
        perror("DroneBridgeCommon: SIOCGIFHWADDR");
        new_socket.db_socket = -1;
        return new_socket;
    }
    socket_fd = bindsocket(socket_fd, trans_mode, ifName);
    if (trans_mode == 'w') {
        LOG_SYS_STD(LOG_ERR, "DroneBridgeCommon: Wifi mode is not yet supported!\n");
        new_socket.db_socket = -1;
        return new_socket;
        //return conf_ethernet(dest_mac);
    } else {
        new_socket.db_socket = conf_monitor(socket_fd, comm_id, bitrate_option, send_direction, receive_new_port,
                                            frame_type);
        new_socket.db_socket_addr = socket_address;
        return new_socket;
    }
}

/**
 * Increases/Updates an existing sequence number so that it can be used to send a new packet over long range socket.
 * Sequence numbers range from 0-255
 * @param old_seq_num A pointer to the sequence number
 * @return The updated sequence number
 */
uint8_t update_seq_num(uint8_t *old_seq_num) {
    (*old_seq_num == 255) ? *old_seq_num = 0 : (*old_seq_num)++;
    return *old_seq_num;
}

/**
 * Returns a pointer to the buffer that gets sent when calling send_packet_hp(..) or send_packet_hp_div(...)
 * @param adhere_to_80211_header: Set to 1 to enable. Offsets the payload by some bytes so that it sits outside the
 * 802.11 header. This is required since some drivers (Ubuntu Atheros drivers) write a sequence number to the 802.11
 * header on receive. Without offsetting the payload this sequence number would overwrite 2 bytes of the payload and
 * corrupt it. The receiver will be able to auto detect the offset. Set this to 1 if you are using a non DB-Rasp Kernel!
 * This will make the packet longer!
 * @return: A pointer to the buffer that gets sent when calling send_packet_hp(..) or send_packet_hp_div(...)
 */
struct data_uni *get_hp_raw_buffer(int adhere_to_80211_header) {
    if (adhere_to_80211_header) {
        db_raw_offset = DB_RAW_OFFSET;
        return (struct data_uni *) (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DB_RAW_OFFSET);
    } else
        return (struct data_uni *) (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
}

/**
 * This function works the same as send_packet with the difference that it allows for soft. diversity transmission.
 * You can specify a socket (bound to an interface) that should be used to send the packet.
 * Overwrites payload set inside monitor_framebuffer with the provided payload.
 * @param payload: The payload bytes of the message to be sent. Does use memcpy to write payload into buffer.
 * @param dest_port: The DroneBridge destination port of the message (see db_protocol.h)
 * @param payload_length: The length of the payload in bytes
 * @param new_seq_num: Specify the sequence number of the packet
 * @param adhere_80211_header: Set to 1 to enable. Offsets the payload by some bytes so that it sits outside the
 *                               802.11 header. Set this to 1 if you are using a non DB-Rasp Kernel!
 * @return: 0 on success or -1 on failure
 */
int db_send_div(db_socket_t *a_db_socket, uint8_t *payload, uint8_t dest_port, u_int16_t payload_length,
                uint8_t new_seq_num, int adhere_80211_header) {
    db_raw_header->payload_length[0] = (uint8_t) (payload_length & (uint8_t) 0xFF);
    db_raw_header->payload_length[1] = (uint8_t) ((payload_length >> (uint8_t) 8) & (uint8_t) 0xFF);
    db_raw_header->port = dest_port;
    db_raw_header->seq_num = new_seq_num;
    struct data_uni *monitor_databuffer_internal = get_hp_raw_buffer(adhere_80211_header);
    memcpy(monitor_databuffer_internal->bytes, payload, payload_length);
    if (sendto(a_db_socket->db_socket, monitor_framebuffer, (size_t) (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH +
                                                                      payload_length + db_raw_offset), 0,
               (struct sockaddr *) &a_db_socket->db_socket_addr, sizeof(struct sockaddr_ll)) < 0) {
        LOG_SYS_STD(LOG_ERR, "DroneBridgeCommon: Send failed (monitor): %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * This function works the same as send_packet_hp with the difference that it allows for soft. diversity transmission.
 * You can specify a socket (bound to an interface) that should be used to send the packet.
 * Use this function for maximum performance. No memcpy used. Create a pointer directly pointing inside the
 * monitor_framebuffer array in your script. Fill that with your payload and call this function.
 * This function only sends the monitor_framebuffer. You need to make sure you get the payload inside it.
 * E.g. This will create such a pointer structure:
 *
 *     struct data_uni *data_uni_to_ground = (struct data_uni *)
 *           (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
 *     memset(data_uni_to_ground->bytes, 0xff, DATA_UNI_LENGTH); // set some payload
 *
 * Make sure you update your data every time before sending.
 * @param dest_port The DroneBridge destination port of the message (see db_protocol.h)
 * @param payload_length The length of the payload in bytes
 * @param new_seq_num Specify the sequence number of the packet
 * @return 0 on success or -1 on failure
 */
int db_send_hp_div(db_socket_t *a_db_socket, uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num) {
    db_raw_header->payload_length[0] = (uint8_t) (payload_length & (uint8_t) 0xFF);
    db_raw_header->payload_length[1] = (uint8_t) ((payload_length >> (uint8_t) 8) & (uint8_t) 0xFF);
    db_raw_header->port = dest_port;
    db_raw_header->seq_num = new_seq_num;
    if (sendto(a_db_socket->db_socket, monitor_framebuffer,
               (size_t) (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + payload_length + db_raw_offset), 0,
               (struct sockaddr *) &a_db_socket->db_socket_addr, sizeof(struct sockaddr_ll)) < 0) {
        LOG_SYS_STD(LOG_ERR, "DroneBridgeCommon: Send failed (monitor): %s\n", strerror(errno));
        return -1;
    }
    return 0;
}