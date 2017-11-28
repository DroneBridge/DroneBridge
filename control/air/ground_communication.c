//
// Created by Wolfgang Christl on 26.11.17.
//

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
#include "main.h"
#include "db_protocol.h"

struct ifreq if_idx;
struct ifreq if_mac;
char interfaceName[IFNAMSIZ];
struct sockaddr_ll socket_address;
char mode = 'm';
int socket_ground_comm;

uint8_t monitor_framebuffer_uni[RADIOTAP_LENGTH + AB80211_LENGTH + DATA_UNI_LENGTH];
struct radiotap_header *rth_uni = (struct radiotap_header *) monitor_framebuffer_uni;
struct db_80211_header *db802_uni = (struct db_80211_header *) (monitor_framebuffer_uni + RADIOTAP_LENGTH);
struct data_uni *monitor_databuffer_uni = (struct data_uni *) (monitor_framebuffer_uni + RADIOTAP_LENGTH + AB80211_LENGTH);

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
            fprintf(stderr,"DB_CONTROL_AIR: Wrong bitrate option\n");
            exit(1);
    }

}

int conf_monitor(uint8_t comm_id[4], int bitrate_option, int frame_type, uint8_t direction) {
    memset(monitor_framebuffer_uni, 0, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_UNI_LENGTH));
    set_bitrate(bitrate_option);
    memcpy(rth_uni->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    // build custom DroneBridge 802.11 header
    if (frame_type == 1) {
        memcpy(db802_uni->frame_control_field, frame_control_pre_data, 4);
    } else {
        memcpy(db802_uni->frame_control_field, frame_control_pre_beacon, 4);
    }
    memcpy(db802_uni->comm_id, comm_id, 4);
    db802_uni->odd = 0x01;
    db802_uni->direction_dstmac = direction;
    memcpy(db802_uni->src_mac_bytes, ((uint8_t *) &if_mac.ifr_hwaddr.sa_data), 6);
    db802_uni->version_bytes = DB_VERSION;
    db802_uni->direction_bytes = direction;
    db802_uni->undefined[0] = 0x10;
    db802_uni->undefined[1] = 0x86;
    uint8_t crc8_db_header = 0;
    crc8_db_header ^= (db802_uni->version_bytes & 0xFF);
    crc8_db_header ^= (db802_uni->port_bytes & 0xFF);
    crc8_db_header ^= (db802_uni->direction_bytes & 0xFF);
    crc8_db_header ^= (db802_uni->payload_length_bytes[0] & 0xFF);
    crc8_db_header ^= (db802_uni->payload_length_bytes[1] & 0xFF);
    db802_uni->crc_bytes = crc8_db_header;
    if (setsockopt(socket_ground_comm, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, IFNAMSIZ) < 0) {
        printf("DB_CONTROL_AIR: Error binding monitor socket to interface. Closing socket. Please restart.\n");
        close(socket_ground_comm);
        return 2;
    }
    /* Index of the network device */
    socket_address.sll_ifindex = if_idx.ifr_ifindex;

    return 0;
}

int open_socket_sending(char ifName[IFNAMSIZ], uint8_t comm_id[4], char trans_mode, int bitrate_option, int frame_type,
                        uint8_t direction){
    mode = trans_mode;

    if (mode == 'w') {
        // TODO: ignore. UDP in future
        if ((socket_ground_comm = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("DB_CONTROL_AIR: socket");
            return 2;
        }else{
            printf("DB_CONTROL_AIR: Opened socket for wifi mode\n");
        }

    } else {
        if ((socket_ground_comm = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
            perror("DB_CONTROL_AIR: socket");
            return 2;
        }else{
            printf("DB_CONTROL_AIR: Opened raw socket for monitor mode\n");
        }
    }

    /* Get the index of the interface to send on */
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_ground_comm, SIOCGIFINDEX, &if_idx) < 0) {
        perror("DB_CONTROL_AIR: SIOCGIFINDEX");
        return -1;
    }
    /* Get the MAC address of the interface to send on */
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(socket_ground_comm, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("DB_CONTROL_AIR: SIOCGIFHWADDR");
        return -1;
    }

    if (trans_mode == 'w') {
        printf("DB_CONTROL_AIR: Wifi mode is not yet supported!\n");
        return -1;
        //return conf_ethernet(dest_mac);
    } else {
        return conf_monitor(comm_id, bitrate_option, frame_type, direction);
    }
}

int send_packet(const int8_t payload[], const uint8_t dest_port){
    uint16_t payload_size = sizeof(payload);
    db802_uni->payload_length_bytes[0] = (uint8_t) (payload_size & 0xFF);
    db802_uni->payload_length_bytes[1] = (uint8_t) ((payload_size >> 8) & 0xFF);
    db802_uni->port_bytes = dest_port;
    memcpy(monitor_databuffer_uni->bytes, payload, payload_size);
    if (sendto(socket_ground_comm, monitor_framebuffer_uni, (size_t) (RADIOTAP_LENGTH + AB80211_LENGTH + payload_size), 0,
               (struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
        printf("DB_CONTROL_AIR: Send failed (monitor): %s\n", strerror(errno));
    }
}

void close_socket_ground_comm(){
    close(socket_ground_comm);
}