//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//
/**
 * ----------- Attention -----------
 * Only open one(!) send & receive socket. Multiple instances may will use same variables. Will cause unexpected behaviour.
 * One instance is enought for sending as you can specify the DroneBridge port with every transmit
 * If you need multiple open ports (aka multiple receive sockets) use db_raw_receive.c to open such extra sockets. These
 * sockets will only be able to receive data
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

uint8_t radiotap_header_pre[] = {

        0x00, 0x00, // <-- radiotap version
        0x0d, 0x00, // <- radiotap header length
        0x04, 0x80, 0x00, 0x00, // <-- bitmap
        0x18,       // data rate (will be overwritten)
        0x00,
        0x00, 0x00, 0x00
};
const uint8_t frame_control_pre_data[] =
        {
                0x08, 0x00, 0x00, 0x00
        };
const uint8_t frame_control_pre_beacon[] =
        {
                0x80, 0x00, 0x00, 0x00
        };
const uint8_t frame_control_pre_rts[] =
        {
                0xb4, 0x00, 0x00, 0x00
        };

struct ifreq raw_if_idx;
struct ifreq raw_if_mac;
char interfaceName[IFNAMSIZ];
struct sockaddr_ll socket_address;
char mode = 'm';
int socket_send_receive;

uint8_t monitor_framebuffer[RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DATA_UNI_LENGTH] = {0};
struct radiotap_header *rth = (struct radiotap_header *) monitor_framebuffer;
struct db_raw_v2_header *db_raw_header = (struct db_raw_v2_header *) (monitor_framebuffer + RADIOTAP_LENGTH);
struct data_uni *monitor_databuffer_internal = (struct data_uni *) (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);

/**
 * Set the transmission bit rate in the radiotap header. Only works with ralink cards.
 * @param bitrate_option
 */
void set_bitrate(int bitrate_option) {
    switch (bitrate_option){
        case 1:
            radiotap_header_pre[8] = 0x05;
            break;
        case 2:
            radiotap_header_pre[8] = 0x09;
            break;
        case 3:
            radiotap_header_pre[8] = 0x0c;
            break;
        case 4:
            radiotap_header_pre[8] = 0x18;
            break;
        case 5:
            radiotap_header_pre[8] = 0x24;
            break;
        default:
            fprintf(stderr,"DB_SEND: Wrong bitrate option\n");
            exit(1);
    }

}

/**
 * Setup of the the DroneBridge raw protocol v2 header
 * @param comm_id
 * @param bitrate_option
 * @param frame_type
 * @param send_direction
 * @return The socket file descriptor in case of a success or -1 if we screwed up
 */
int conf_monitor_v2(uint8_t comm_id, int bitrate_option, uint8_t send_direction, uint8_t new_port) {
    memset(monitor_framebuffer, 0, (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH + DATA_UNI_LENGTH));
    set_bitrate(bitrate_option);
    memcpy(rth->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    // build custom DroneBridge v2 header
    memcpy(db_raw_header->fcf_duration, frame_control_pre_rts, 4);
    db_raw_header->direction = send_direction;
    db_raw_header->comm_id = comm_id;
    if (setsockopt(socket_send_receive, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, IFNAMSIZ) < 0) {
        printf("DB_SEND: Error binding monitor socket to interface. Closing socket. Please restart.\n");
        close(socket_send_receive);
        return -1;
    }
    /* Index of the network device */
    socket_address.sll_ifindex = raw_if_idx.ifr_ifindex;
    uint8_t recv_direction = (uint8_t) ((send_direction == DB_DIREC_DRONE) ? DB_DIREC_GROUND : DB_DIREC_DRONE);
    socket_send_receive = setBPF(socket_send_receive, comm_id, recv_direction, new_port);
    return socket_send_receive;
}

/**
 * ONLY OPEN ONE INSTANCE OF THIS SOCKET!
 *
 * Opens and configures a socket for sending and receiving DroneBridge raw protocol frames.
 * @param ifName Name of the network interface the socket is bound to
 * @param comm_id The communication ID
 * @param trans_mode The transmission mode (m|w) for monitor or wifi
 * @param bitrate_option Transmission bit rate. Only works with Ralink cards
 * @param send_direction Are the sent packets for the drone or the groundstation
 * @param receive_new_port Port the BPF filter gets set to. Port open for receiving data.
 * @return the socket file descriptor or -1 if something went wrong
 */
int open_socket_send_receive(char *ifName, uint8_t comm_id, char trans_mode, int bitrate_option,
                             uint8_t send_direction, uint8_t receive_new_port){
    mode = trans_mode;

    if (mode == 'w') {
        // TODO: ignore for now. I will be UDP in future.
        if ((socket_send_receive = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("DB_SEND: socket");
            return -1;
        }else{
            printf("DB_SEND: Opened socket for wifi mode\n");
        }

    } else {
        if ((socket_send_receive = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
            perror("DB_SEND: socket");
            return -1;
        }else{
            printf("DB_SEND: Opened raw socket for monitor mode\n");
        }
    }

    /* Get the index of the interface to send on */
    memset(&raw_if_idx, 0, sizeof(struct ifreq));
    strncpy(raw_if_idx.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_send_receive, SIOCGIFINDEX, &raw_if_idx) < 0) {
        perror("DB_SEND: SIOCGIFINDEX");
        return -1;
    }
    /* Get the MAC address of the interface to send on */
    memset(&raw_if_mac, 0, sizeof(struct ifreq));
    strncpy(raw_if_mac.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_send_receive, SIOCGIFHWADDR, &raw_if_mac) < 0) {
        perror("DB_SEND: SIOCGIFHWADDR");
        return -1;
    }
    socket_send_receive = bindsocket(socket_send_receive, trans_mode, ifName);
    if (trans_mode == 'w') {
        printf("DB_SEND: Wifi mode is not yet supported!\n");
        return -1;
        //return conf_ethernet(dest_mac);
    } else {
        return conf_monitor_v2(comm_id, bitrate_option, send_direction, receive_new_port);
    }
}

uint8_t update_seq_num(uint8_t *old_seq_num){
    (*old_seq_num == 255) ? *old_seq_num = 0 : (*old_seq_num)++;
    return *old_seq_num;
}

/**
 * Overwrites payload set inside monitor_framebuffer with the provided payload.
 * @param payload The payload bytes of the message to be sent. Does use memcpy to write payload into buffer.
 * @param dest_port The DroneBridge destination port of the message (see db_protocol.h)
 * @param payload_length The length of the payload in bytes
 * @param new_seq_num Specify the seqence number of the packet
 * @return 0 if success or -1 if failure
 */
int send_packet(int8_t payload[], uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num){
    db_raw_header->payload_length[0] = (uint8_t) (payload_length & 0xFF);
    db_raw_header->payload_length[1] = (uint8_t) ((payload_length >> 8) & 0xFF);
    db_raw_header->port = dest_port;
    db_raw_header->seq_num = new_seq_num;
    memcpy(monitor_databuffer_internal->bytes, payload, payload_length);
    if (sendto(socket_send_receive, monitor_framebuffer, (size_t) (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH +
            payload_length), 0, (struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
        printf("DB_SEND: Send failed (monitor): %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * Use this function for maximum performance. No memcpy used. Create a pointer directly pointing inside the
 * monitor_framebuffer array in your script. Fill that with your payload and call this function.
 * This function only sends the monitor_framebuffer. You need to make sure you get the payload inside it.
 * E.g. This will create such a pointer structure:
 * struct data_rc_status_update *rc_status_update_data = (struct data_rc_status_update *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
 * Make sure you update your data every time before sending.
 * @param dest_port The DroneBridge destination port of the message (see db_protocol.h)
 * @param payload_length The length of the payload in bytes
 * @param new_seq_num Specify the seqence number of the packet
 * @return 0 if success or -1 if failure
 */
int send_packet_hp(uint8_t dest_port, u_int16_t payload_length, uint8_t new_seq_num){
    db_raw_header->payload_length[0] = (uint8_t) (payload_length & 0xFF);
    db_raw_header->payload_length[1] = (uint8_t) ((payload_length >> 8) & 0xFF);
    db_raw_header->port = dest_port;
    db_raw_header->seq_num = new_seq_num;
    if (sendto(socket_send_receive, monitor_framebuffer, (size_t) (RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH +
            payload_length), 0, (struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
        printf("DB_SEND: Send failed (monitor): %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void close_socket_send_receive(){
    close(socket_send_receive);
}