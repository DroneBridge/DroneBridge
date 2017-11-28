#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>
#include "db_protocol.h"
#include "main.h"
#include "parameter.h"

int sockfd, rc_protocol;
struct ifreq if_idx;
struct ifreq if_mac;
int tx_len = 0, b = 0;
char interfaceName[IFNAMSIZ];

//uint8_t Framebuf[HEADERBUF_SIZ + DATA_LENTH];
//uint8_t Framebuf_v2[HEADERBUF_SIZ + DATA_LENTH_V2];
//struct ether_header *eh = (struct ether_header *) Framebuf;
//struct data *databuffer = (struct data *) (Framebuf + sizeof(struct ether_header));
struct sockaddr_ll socket_address;

uint8_t monitor_framebuffer[RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH];
uint8_t monitor_framebuffer_v2[RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH_V2];
uint8_t monitor_framebuffer_uni[RADIOTAP_LENGTH + AB80211_LENGTH + DATA_UNI_LENGTH];

struct radiotap_header *rth = (struct radiotap_header *) monitor_framebuffer;
struct radiotap_header *rth_v2 = (struct radiotap_header *) monitor_framebuffer_v2;
struct radiotap_header *rth_uni = (struct radiotap_header *) monitor_framebuffer_uni;

struct db_80211_header *db802 = (struct db_80211_header *) (monitor_framebuffer + RADIOTAP_LENGTH);
struct db_80211_header *db802_v2 = (struct db_80211_header *) (monitor_framebuffer_v2 + RADIOTAP_LENGTH);
struct db_80211_header *db802_uni = (struct db_80211_header *) (monitor_framebuffer_uni + RADIOTAP_LENGTH);


struct data *monitor_databuffer = (struct data *) (monitor_framebuffer + RADIOTAP_LENGTH + AB80211_LENGTH);
struct datav2 *monitor_databuffer_v2 = (struct datav2 *) (monitor_framebuffer_v2 + RADIOTAP_LENGTH + AB80211_LENGTH);
struct data_uni *monitor_databuffer_uni = (struct data_uni *) (monitor_framebuffer_uni + RADIOTAP_LENGTH + AB80211_LENGTH);

char mode;
uint8_t crcS2, crc8;

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
            fprintf(stderr,"DB_CONTROL_GROUND: Wrong bitrate option\n");
            exit(1);
    }

}
int openSocket(char ifName[IFNAMSIZ], uint8_t comm_id[4], char trans_mode, int bitrate_option, int frame_type,
               int new_msp_version) {
    mode = trans_mode;
    rc_protocol = new_msp_version;

    if (trans_mode == 'w') {
        // TODO: ignore. UDP in future
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("DB_CONTROL_GROUND: socket");
            return 2;
        }else{
            printf("DB_CONTROL_GROUND: Opened socket for wifi mode\n");
        }

    } else {
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
            perror("DB_CONTROL_GROUND: socket");
            return 2;
        }else{
            printf("DB_CONTROL_GROUND: Opened raw socket for monitor mode\n");
        }
    }

    /* Get the index of the interface to send on */
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("DB_CONTROL_GROUND: SIOCGIFINDEX");
        return -1;
    }
    /* Get the MAC address of the interface to send on */
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, ifName, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("DB_CONTROL_GROUND: SIOCGIFHWADDR");
        return -1;
    }

    if (trans_mode == 'w') {
        printf("DB_CONTROL_GROUND: Wifi mode is not yet supported!\n");
        return -1;
        //return conf_ethernet(dest_mac);
    } else {
        return conf_monitor(comm_id, bitrate_option, frame_type);
    }
}

/*int conf_ethernet(unsigned char dest_mac[6]) {
    // TODO: ignore: UDP in the future
    *//* Construct the Ethernet header *//*
    memset(Framebuf, 0, HEADERBUF_SIZ);
    *//* Ethernet header *//*
    eh->ether_shost[0] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[0];
    eh->ether_shost[1] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[1];
    eh->ether_shost[2] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[2];
    eh->ether_shost[3] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[3];
    eh->ether_shost[4] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[4];
    eh->ether_shost[5] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[5];
    eh->ether_dhost[0] = dest_mac[0];
    eh->ether_dhost[1] = dest_mac[1];
    eh->ether_dhost[2] = dest_mac[2];
    eh->ether_dhost[3] = dest_mac[3];
    eh->ether_dhost[4] = dest_mac[4];
    eh->ether_dhost[5] = dest_mac[5];
    *//* Ethertype field *//*
    eh->ether_type = htons(ETHER_TYPE);
    tx_len += sizeof(struct ether_header);

    *//* Index of the network device *//*
    socket_address.sll_ifindex = if_idx.ifr_ifindex;
    *//* Address length*//*
    socket_address.sll_halen = ETH_ALEN;
    *//* Destination MAC *//*
    socket_address.sll_addr[0] = dest_mac[0];
    socket_address.sll_addr[1] = dest_mac[1];
    socket_address.sll_addr[2] = dest_mac[2];
    socket_address.sll_addr[3] = dest_mac[3];
    socket_address.sll_addr[4] = dest_mac[4];
    socket_address.sll_addr[5] = dest_mac[5];
    return 0;
}*/

int conf_monitor(uint8_t comm_id[4], int bitrate_option, int frame_type) {
    memset(monitor_framebuffer, 0, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH));
    memset(monitor_framebuffer_v2, 0, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH_V2));
    set_bitrate(bitrate_option);
    memcpy(rth->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    memcpy(rth_v2->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    // build custom DroneBridge 802.11 header
    if (frame_type==1){
        memcpy(db802->frame_control_field, frame_control_pre_data, 4);
        memcpy(db802_v2->frame_control_field, frame_control_pre_data, 4);
    }else{
        memcpy(db802->frame_control_field, frame_control_pre_beacon, 4);
        memcpy(db802_v2->frame_control_field, frame_control_pre_beacon, 4);
    }
    // We init the buffers for MSP v1 and v2 even if we are only using only one of them. Quick and "dirty"
    // init for MSP v1
    memcpy(db802->comm_id, comm_id, 4);
    db802->odd = 0x01; // for some (strange?) reason it can not be 0x00 so we make sure
    db802->direction_dstmac = DB_DIREC_DRONE;
    memcpy(db802->src_mac_bytes, ((uint8_t *) &if_mac.ifr_hwaddr.sa_data), 6);
    db802->version_bytes = DB_VERSION;
    db802->port_bytes = DB_PORT_CONTROLLER;
    db802->direction_bytes = DB_DIREC_DRONE;
    db802->payload_length_bytes[0] = 0x22; //TODO auto detect = DATA_LENGTH
    db802->payload_length_bytes[1] = 0x00;
    db802->crc_bytes = CRC_RC_TO_DRONE;
    db802->undefined[0] = 0x10;
    db802->undefined[1] = 0x86;
    // Init for MSP v2
    memcpy(db802_v2->comm_id, comm_id, 4);
    db802_v2->odd = 0x01; // for some (strange?) reason it can not be 0x00 so we make sure
    db802_v2->direction_dstmac = DB_DIREC_DRONE;
    memcpy(db802_v2->src_mac_bytes, ((uint8_t *) &if_mac.ifr_hwaddr.sa_data), 6);
    db802_v2->version_bytes = DB_VERSION;
    db802_v2->port_bytes = DB_PORT_CONTROLLER;
    db802_v2->direction_bytes = DB_DIREC_DRONE;
    db802_v2->payload_length_bytes[0] = 0x25; //TODO auto detect = DATA_LENGTH_V2 make it htons(int16_t)
    db802_v2->payload_length_bytes[1] = 0x00;
    db802_v2->crc_bytes = CRC_RC_TO_DRONE;
    db802_v2->undefined[0] = 0x10;
    db802_v2->undefined[1] = 0x86;
    // Init for universal payload
    memcpy(db802_v2->comm_id, comm_id, 4);
    db802_v2->odd = 0x01; // for some (strange?) reason it can not be 0x00 so we make sure
    db802_v2->direction_dstmac = DB_DIREC_DRONE;
    memcpy(db802_v2->src_mac_bytes, ((uint8_t *) &if_mac.ifr_hwaddr.sa_data), 6);
    db802_v2->version_bytes = DB_VERSION;
    db802_v2->port_bytes = DB_PORT_CONTROLLER;
    db802_v2->direction_bytes = DB_DIREC_DRONE;
    db802_v2->crc_bytes = CRC_RC_TO_DRONE;
    db802_v2->undefined[0] = 0x10;
    db802_v2->undefined[1] = 0x86;


    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, IFNAMSIZ) < 0) {
        printf("DB_CONTROL_GROUND: Error binding monitor socket to interface. Closing socket. Please restart.\n");
        close(sockfd);
        return 2;
    }

    /* Index of the network device */
    socket_address.sll_ifindex = if_idx.ifr_ifindex;

    return 0;
}

// could do this with two for-loops but hardcoded is faster and number of aux channels won't change anyways
void generate_msp(unsigned short *newJoystickData) {
    monitor_databuffer->bytes[0] = 0x24;
    monitor_databuffer->bytes[1] = 0x4d;
    monitor_databuffer->bytes[2] = 0x3c;
    monitor_databuffer->bytes[3] = 0x1c;
    monitor_databuffer->bytes[4] = 0xc8;
    //Roll
    monitor_databuffer->bytes[5] = ((newJoystickData[0] >> 8) & 0xFF);
    monitor_databuffer->bytes[6] = newJoystickData[0] & 0xFF;
    //Pitch
    monitor_databuffer->bytes[7] = (newJoystickData[1] >> 8) & 0xFF;
    monitor_databuffer->bytes[8] = newJoystickData[1] & 0xFF;
    //Yaw
    monitor_databuffer->bytes[9] = (newJoystickData[2] >> 8) & 0xFF;
    monitor_databuffer->bytes[10] = newJoystickData[2] & 0xFF;
    //Throttle
    monitor_databuffer->bytes[11] = (newJoystickData[3] >> 8) & 0xFF;
    monitor_databuffer->bytes[12] = newJoystickData[3] & 0xFF;
    //AUX 1
    monitor_databuffer->bytes[13] = (newJoystickData[4] >> 8) & 0xFF;
    monitor_databuffer->bytes[14] = newJoystickData[4] & 0xFF;
    //AUX 2
    monitor_databuffer->bytes[15] = (newJoystickData[5] >> 8) & 0xFF;
    monitor_databuffer->bytes[16] = newJoystickData[5] & 0xFF;
    //AUX 3
    monitor_databuffer->bytes[17] = (newJoystickData[6] >> 8) & 0xFF;
    monitor_databuffer->bytes[18] = newJoystickData[6] & 0xFF;
    //AUX 4
    monitor_databuffer->bytes[19] = (newJoystickData[7] >> 8) & 0xFF;
    monitor_databuffer->bytes[20] = newJoystickData[7] & 0xFF;
    //AUX 5
    monitor_databuffer->bytes[21] = (newJoystickData[8] >> 8) & 0xFF;
    monitor_databuffer->bytes[22] = newJoystickData[8] & 0xFF;
    //AUX 6
    monitor_databuffer->bytes[23] = (newJoystickData[9] >> 8) & 0xFF;
    monitor_databuffer->bytes[24] = newJoystickData[9] & 0xFF;
    //AUX 7
    monitor_databuffer->bytes[25] = (newJoystickData[10] >> 8) & 0xFF;
    monitor_databuffer->bytes[26] = newJoystickData[10] & 0xFF;
    //AUX 8
    monitor_databuffer->bytes[27] = (newJoystickData[11] >> 8) & 0xFF;
    monitor_databuffer->bytes[28] = newJoystickData[11] & 0xFF;
    //AUX 9
    monitor_databuffer->bytes[29] = (newJoystickData[12] >> 8) & 0xFF;
    monitor_databuffer->bytes[30] = newJoystickData[12] & 0xFF;
    //AUX 10
    monitor_databuffer->bytes[31] = ((newJoystickData[13] >> 8) & 0xFF);
    monitor_databuffer->bytes[32] = newJoystickData[13] & 0xFF;
    // CRC
    crc8 = 0;
    for (int i = 3; i < 33; i++) {
        crc8 ^= (monitor_databuffer->bytes[i] & 0xFF);
    }
    monitor_databuffer->bytes[33] = crc8;
//    printf("MSP Data: ");
//    for(int i = 0;i<sizeof(databuffer->bytes);i++){
//        printf(" %02x",databuffer->bytes[i]);
//    }
//    printf("\n");
//    printf("---------");
}

uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a)
{
    crc ^= a;
    for (int ii = 0; ii < 8; ++ii) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0xD5;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

void generate_mspv2(unsigned short *newJoystickData) {
    monitor_databuffer_v2->bytes[0] = 0x24;
    monitor_databuffer_v2->bytes[1] = 0x58;
    monitor_databuffer_v2->bytes[2] = 0x3c;
    monitor_databuffer_v2->bytes[3] = 0x00; // flag
    monitor_databuffer_v2->bytes[4] = 0xc8; // function
    monitor_databuffer_v2->bytes[5] = 0x00; // function
    monitor_databuffer_v2->bytes[6] = 0x1c; // payload size
    monitor_databuffer_v2->bytes[7] = 0x00; // payload size
    //Roll
    monitor_databuffer_v2->bytes[8] = (uint8_t) ((newJoystickData[0] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[9] = (uint8_t) (newJoystickData[0] & 0xFF);
    //Pitch
    monitor_databuffer_v2->bytes[10] = (uint8_t) ((newJoystickData[1] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[11] = (uint8_t) ( newJoystickData[1] & 0xFF);
    //Yaw
    monitor_databuffer_v2->bytes[12] = (uint8_t) ((newJoystickData[2] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[13] = (uint8_t) (newJoystickData[2] & 0xFF);
    //Throttle
    monitor_databuffer_v2->bytes[14] = (uint8_t) ((newJoystickData[3] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[15] = (uint8_t) (newJoystickData[3] & 0xFF);
    //AUX 1
    monitor_databuffer_v2->bytes[16] = (uint8_t) ((newJoystickData[4] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[17] = (uint8_t) (newJoystickData[4] & 0xFF);
    //AUX 2
    monitor_databuffer_v2->bytes[18] = (uint8_t) ((newJoystickData[5] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[19] = (uint8_t) (newJoystickData[5] & 0xFF);
    //AUX 3
    monitor_databuffer_v2->bytes[20] = (uint8_t) ((newJoystickData[6] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[21] = (uint8_t) (newJoystickData[6] & 0xFF);
    //AUX 4
    monitor_databuffer_v2->bytes[22] = (uint8_t) ((newJoystickData[7] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[23] = (uint8_t) (newJoystickData[7] & 0xFF);
    //AUX 5
    monitor_databuffer_v2->bytes[24] = (uint8_t) ((newJoystickData[8] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[25] = (uint8_t) (newJoystickData[8] & 0xFF);
    //AUX 6
    monitor_databuffer_v2->bytes[26] = (uint8_t) ((newJoystickData[9] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[27] = (uint8_t) (newJoystickData[9] & 0xFF);
    //AUX 7
    monitor_databuffer_v2->bytes[28] = (uint8_t) ((newJoystickData[10] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[29] = (uint8_t) (newJoystickData[10] & 0xFF);
    //AUX 8
    monitor_databuffer_v2->bytes[30] = (uint8_t) ((newJoystickData[11] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[31] = (uint8_t) (newJoystickData[11] & 0xFF);
    //AUX 9
    monitor_databuffer_v2->bytes[32] = (uint8_t) ((newJoystickData[12] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[33] = (uint8_t) (newJoystickData[12] & 0xFF);
    //AUX 10
    monitor_databuffer_v2->bytes[34] = (uint8_t) ((newJoystickData[13] >> 8) & 0xFF);
    monitor_databuffer_v2->bytes[35] = (uint8_t) (newJoystickData[13] & 0xFF);
    // CRC
    crcS2 = 0;
    for(int i = 3; i < 36; i++)
        crcS2 = crc8_dvb_s2(crcS2, monitor_databuffer_v2->bytes[i]);
    monitor_databuffer_v2->bytes[36] = crcS2;
//    printf("MSPv2 Data: ");
//    for(int i = 0;i<sizeof(databuffer->bytes);i++){
//        printf(" %02x",databuffer->bytes[i]);
//    }
//    printf("\n");
//    printf("---------");
}

void generate_mavlink(unsigned short newJoystickData[NUM_CHANNELS]) {

}

int sendPacket(unsigned short contData[]) {
    // TODO check for RC overwrite!
    // There might be a nicer way of doing things but this one is fast and easily added (two different buffers)
    if (rc_protocol == 1){
        generate_msp(contData);
        if (sendto(sockfd, monitor_framebuffer, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH), 0,
                   (struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
            printf("DB_CONTROL_GROUND: Send failed (monitor mspv1): %s\n", strerror(errno));
        }
    }else if (rc_protocol == 2){
        generate_mspv2(contData);
        if (sendto(sockfd, monitor_framebuffer_v2, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH_V2), 0,
                   (struct sockaddr *) &socket_address, sizeof(struct sockaddr_ll)) < 0) {
            printf("DB_CONTROL_GROUND: Send failed (monitor mspv2): %s\n", strerror(errno));
        }
    }else{
        generate_mavlink(contData);
        // TODO: set db_payload length, send MAVLink
    }

    //printf( "%c[;H", 27 );
//    printf("\n");
//    for(int i = (RADIOTAP_LENGTH + AB80211_LENGTH);i< (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH);i++){
//        printf(" %02x",monitor_framebuffer[i]);
//    }
    //printf("---------");
    //printf("Databuffer: %x",databuffer);

    return 0;
}

void closeSocket() {
    close(sockfd);
}
