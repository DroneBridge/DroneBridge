/*
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 */
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <errno.h>
#include "db_protocol.h"
#include "main.h"

    int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
	int tx_len = 0;
	char interfaceName[IFNAMSIZ];

	unsigned char Framebuf[HEADERBUF_SIZ + DATA_LENTH];
	struct ether_header *eh = (struct ether_header *) Framebuf;
    struct data *databuffer = (struct data *) (Framebuf + sizeof(struct ether_header));
	struct sockaddr_ll socket_address;

	unsigned char monitor_framebuffer[RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH];
	struct radiotap_header *rth = (struct radiotap_header *) monitor_framebuffer;
	struct ab_80211_header *ab802 = (struct ab_80211_header *) (monitor_framebuffer + RADIOTAP_LENGTH);
	struct data *monitor_databuffer = (struct data *) (monitor_framebuffer + RADIOTAP_LENGTH + AB80211_LENGTH);

	char mode;
    unsigned char crc;
    //struct crcdata *thecrcdata;
    struct crcdata *thecrcdata = (struct crcdata *) (MSPbuf + 3);

int openSocket(char ifName[16], unsigned char dest_mac[6], char trans_mode){
    mode = trans_mode;
    strncpy(interfaceName, ifName, IFNAMSIZ-1);

    if(trans_mode == 'w'){
        // TODO: ignore. UDP in future
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
            perror("socket");
            return 2;
        }
        printf("Opened raw socket for wifi/ethernet mode\n");
    }else{
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
            perror("socket");
            return 2;
        }
        printf("Opened raw socket for monitor mode\n");
    }

    /* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0){perror("SIOCGIFINDEX");return 0;}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0){perror("SIOCGIFHWADDR");return 0;}

    if(trans_mode == 'w'){
        return conf_ethernet(dest_mac);
    }else{
        return conf_monitor(dest_mac);
    }
}

int conf_ethernet(unsigned char dest_mac[6]){
    // TODO: ignore: UDP in future
	/* Construct the Ethernet header */
	memset(Framebuf, 0, HEADERBUF_SIZ);
	/* Ethernet header */
	eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
	eh->ether_dhost[0] = dest_mac[0];
	eh->ether_dhost[1] = dest_mac[1];
	eh->ether_dhost[2] = dest_mac[2];
	eh->ether_dhost[3] = dest_mac[3];
	eh->ether_dhost[4] = dest_mac[4];
	eh->ether_dhost[5] = dest_mac[5];
	/* Ethertype field */
	eh->ether_type = htons(ETHER_TYPE);
	tx_len += sizeof(struct ether_header);

	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = dest_mac[0];
	socket_address.sll_addr[1] = dest_mac[1];
	socket_address.sll_addr[2] = dest_mac[2];
	socket_address.sll_addr[3] = dest_mac[3];
	socket_address.sll_addr[4] = dest_mac[4];
    socket_address.sll_addr[5] = dest_mac[5];
    return 0;
}

int conf_monitor(unsigned char dest_mac[6]){
    memset(monitor_framebuffer, 0, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH));

    memcpy(rth->bytes, radiotap_header_pre, RADIOTAP_LENGTH);
    // build custom AirBridge 802.11 header
    memcpy(ab802->frame_control_field, frame_control_pre, 4);
    memcpy(ab802->dest_mac_bytes, dest_mac, 6);
    ab802->dest_mac_bytes[0] = 0x01; // for some (strange?) reason it can not be 0x00 so we make sure
    memcpy(ab802->src_mac_bytes, ((uint8_t *)&if_mac.ifr_hwaddr.sa_data), 6);
    ab802->version_bytes = AB_VERSION;
    ab802->port_bytes = AB_PORT_CONTROLLER;
    ab802->direction_bytes = AB_DIREC_DRONE;
    ab802->payload_length_bytes[0] = 0x16;
    ab802->payload_length_bytes[1] = 0x00;
    ab802->crc_bytes = CRC_RC_TO_DRONE;
    ab802->undefined[0] = 0x10;
    ab802->undefined[1] = 0x86;


    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interfaceName, IFNAMSIZ) < 0){
        printf("Error binding monitor socket to interface. Closing socket. Please restart.\n");
        close(sockfd);
        return 2;
    }

    	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;

    return 0;
}

void generateMSP(unsigned short newJoystickData[8]){
    //Roll
    MSPbuf[5] = (newJoystickData[0] >> 8) & 0xFF;
    MSPbuf[6] = newJoystickData[0] & 0xFF;
    //Pitch
    MSPbuf[7] = (newJoystickData[1] >> 8) & 0xFF;
    MSPbuf[8] = newJoystickData[1] & 0xFF;
    //Yaw
    MSPbuf[9] = (newJoystickData[2] >> 8) & 0xFF;
    MSPbuf[10] = newJoystickData[2] & 0xFF;
    //Throttle
    MSPbuf[11] = (newJoystickData[3] >> 8) & 0xFF;
    MSPbuf[12] = newJoystickData[3] & 0xFF;
    //AUX 1
    MSPbuf[13] = (newJoystickData[4] >> 8) & 0xFF;
    MSPbuf[14] = newJoystickData[4] & 0xFF;
    //AUX 2
    MSPbuf[15] = (newJoystickData[5] >> 8) & 0xFF;
    MSPbuf[16] = newJoystickData[5] & 0xFF;
    //AUX 3
    MSPbuf[17] = (newJoystickData[6] >> 8) & 0xFF;
    MSPbuf[18] = newJoystickData[6] & 0xFF;
    //AUX 4
    MSPbuf[19] = (newJoystickData[7] >> 8) & 0xFF;
    MSPbuf[20] = newJoystickData[7] & 0xFF;
    // CRC
    crc=0;
    for (int i = 0; i<sizeof(struct crcdata); i++)
    {
        crc ^= (thecrcdata->bytes[i]&0xFF);
    }
    MSPbuf[21] = crc;
//    printf("MSP Data: ");
//    for(int i = 0;i<sizeof(MSPbuf);i++){
//        printf(" %02x",MSPbuf[i]);
//    }
//    printf("\n");
//    printf("---------");
}

int sendPacket(unsigned short contData[])
{
    generateMSP(contData);
    if(mode == 'w'){
        // TODO: ignore. UDP in future
        memcpy(databuffer->bytes, MSPbuf, DATA_LENTH);
        if (sendto(sockfd, Framebuf, (DATA_LENTH+HEADERBUF_SIZ), 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
            printf("Send failed (wifi): %s\n", strerror(errno));
    }else{
        memcpy(monitor_databuffer->bytes, MSPbuf, DATA_LENTH);
        if(sendto(sockfd, monitor_framebuffer, (RADIOTAP_LENGTH + AB80211_LENGTH + DATA_LENTH), 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0){
            printf("Send failed (monitor): %s\n", strerror(errno));
        }
    }


    //printf("\n");
//    for(int i = 0;i<DATA_LENTH+HEADERBUF_SIZ;i++){
//        printf(" %02x",Framebuf[i]);
//    }
    //printf("---------");
	/* Send packet */
	//printf("Framebuffer: %x %x %x %x %x %x\n",Framebuf[12],Framebuf[11],Framebuf[10],Framebuf[9],Framebuf[8],Framebuf[7]);
	//printf("Databuffer: %x",databuffer);

	return 0;
}

void closeSocket(){
    close(sockfd);
}
