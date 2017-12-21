//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//
/**
 * ----------- Attention -----------
 * These sockets will only be able to receive data! If you need to send data use "db_raw_send_receive.c"
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/filter.h> // BPF
#include <linux/if_packet.h> // sockaddr_ll
#include <fcntl.h> // File control definitions
#include <termios.h> // POSIX terminal control definitionss
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "db_raw_receive.h"
#include "db_protocol.h"

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
int expected_seq_num;

/**
 * Set a BPF filter on the socket (DroneBridge raw protocol v2)
 * @param newsocket The socket file descriptor on which the BPF filter should be set
 * @param new_comm_id The communication ID that we filter for
 * @param direction Packets with what kind of directions (DB_DIREC_DRONE or DB_V2_DIREC_GROUND) are allowed to pass the filter
 * @param port The port of the module using this function. See db_protocol.h (DB_PORT_CONTROLLER, DB_PORT_COMM, ...)
 * @return The socket with set BPF filter
 */
int setBPF(int newsocket, const uint8_t new_comm_id, uint8_t direction, uint8_t port)
{
    struct sock_filter dest_filter[] =
            {
                    { 0x30,  0,  0, 0x00000003 },
                    { 0x64,  0,  0, 0x00000008 },
                    { 0x07,  0,  0, 0000000000 },
                    { 0x30,  0,  0, 0x00000002 },
                    { 0x4c,  0,  0, 0000000000 },
                    { 0x02,  0,  0, 0000000000 },
                    { 0x07,  0,  0, 0000000000 },
                    { 0x48,  0,  0, 0000000000 },
                    { 0x45,  1,  0, 0x0000b400 },   // allow rts frames
                    { 0x45,  0,  5, 0x00000800 },   // allow data frames
                    { 0x48,  0,  0, 0x00000004 },
                    { 0x15,  0,  3, 0x00000301 },   // <direction><comm id>
                    { 0x50,  0,  0, 0x00000006 },
                    { 0x15,  0,  1, 0x00000005 },   // <port>
                    { 0x06,  0,  0, 0x00002000 },
                    { 0x06,  0,  0, 0000000000 },
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
    if (ret < 0)
    {
        perror("DB_RECEIVE: could not attach BPF: ");
        close(newsocket);
        return -1;
    }
    return newsocket;
}

/**
 * Bind the socket to a network interface
 * @param newsocket The socket to be bound
 * @param the_mode The DroneBridge mode we are in (monitor or wifi (unsupported))
 * @param new_ifname The name of the interface we want the socket to bind to
 * @return The socket that is bound to the network interface
 */
int bindsocket(int newsocket, char the_mode, char new_ifname[IFNAMSIZ])
{
    struct sockaddr_ll sll;
    struct ifreq ifr;
    bzero(&sll, sizeof(sll));
    bzero(&ifr, sizeof(ifr));
    strncpy((char *)ifr.ifr_name,new_ifname, IFNAMSIZ-1);
    bzero(&sll, sizeof(sll));
    if((ioctl(newsocket, SIOCGIFINDEX, &ifr)) == -1)
    {
        perror("DB_RECEIVE: Unable to find interface index ");
        return -1;
    }

    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    if(the_mode == 'w')
    {
        sll.sll_protocol = htons(ETHER_TYPE);
    }
    else
    {
        sll.sll_protocol = htons(ETH_P_802_2);
    }
    if((bind(newsocket, (struct sockaddr *)&sll, sizeof(sll))) ==-1)
    {
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
int set_socket_nonblocking(int the_socketfd){
    if(fcntl(the_socketfd, F_SETFL, O_NONBLOCK) < 0){
        perror("Can not put socket in non-blocking mode");
    }else{
        return the_socketfd;
    }
}

/**
 *
 * @param the_socketfd The socket to be set with a timeout
 * @param time_out_s Timeout seconds
 * @param time_out_us Timeout micro seconds
 * @return The socket with a set timeout
 */
int set_socket_timeout(int the_socketfd, int time_out_s ,int time_out_us){
    struct timeval tv_timeout;
    tv_timeout.tv_sec = time_out_s;
    tv_timeout.tv_usec = time_out_us;
    setsockopt(the_socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_timeout, sizeof(struct timeval));
    return the_socketfd;
}

/**
 * Counts the lost packets. Protocol fails if we lost more than 255 packets at once
 * @param last_seq_num The sequence number of the previous packet we received
 * @param received_seq_num The sequence number of the packet we received "right now"
 * @return The number of packets we lost in between the previous packet and the most current one
 */
uint8_t count_lost_packets(uint8_t last_seq_num, uint8_t received_seq_num){
    expected_seq_num = ((last_seq_num == 255) ? 0 : (last_seq_num + 1));
    // Could be one statement. Less confusing this way.
    if (expected_seq_num == received_seq_num) return 0;
    return (uint8_t) ((received_seq_num > expected_seq_num) ? (received_seq_num - expected_seq_num) :
               (255-expected_seq_num)+received_seq_num);
}

/**
 * Create a socket, bind it to a network interface and set the BPF filter. All in one function
 * @param newifName The name of the interface we want the socket to bind to
 * @param new_mode The DroneBridge mode we are in (monitor or wifi (unsupported))
 * @param comm_id The communication ID to set BPF to
 * @param revc_direction packets with what kind of directions (DB_DIREC_DRONE or DB_DIREC_GROUND) are allowed to pass the
 * filter.
 * @param new_port The port of the module using this function. See db_protocol.h (DB_PORT_CONTROLLER, DB_PORT_COMM, ...)
 * @return The socket file descriptor. Socket is bound and has a set BPF filter. Returns -1 if we screwed up.
 */
int open_receive_socket(char newifName[IFNAMSIZ], char new_mode, uint8_t comm_id, uint8_t revc_direction,
                        uint8_t new_port)
{
    int sockfd, sockopt;
    //struct ifreq if_ip;	/* get ip addr */
    struct ifreq ifopts;	/* set promiscuous mode */
    //memset(&if_ip, 0, sizeof(struct ifreq));

    if (new_mode == 'w')
    {
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE))) == -1)
        {
            perror("DB_CONTROL_AIR: error in wifi socket setup\n");
            return -1;
        }
        int flags = fcntl(sockfd,F_GETFL,0);
        fcntl(sockfd, F_SETFL, flags);
        strncpy(ifopts.ifr_name, newifName, IFNAMSIZ-1);
        ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
        ifopts.ifr_flags |= IFF_PROMISC;
        ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
        /* Allow the socket to be reused - incase connection is closed prematurely */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1)
        {
            perror("DB_RECEIVE: setsockopt");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1)
        {
            perror("DB_RECEIVE: error in monitor mode socket setup\n");
            return -1;
        }
        sockfd = setBPF(sockfd, comm_id, revc_direction, new_port);
    }
    sockfd = bindsocket(sockfd, new_mode, newifName);
    return sockfd;
}