#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <termios.h>    // POSIX terminal control definitionss
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include "db_protocol.h"
#include <linux/filter.h>   // BPF
#include <linux/if_packet.h> // sockaddr_ll

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#define ETHER_TYPE	0x88ab
#define DEFAULT_IF      "18a6f716a511"
#define USB_IF          "/dev/ttyACM0"
#define BUF_SIZ		                512 // should be enought?!
#define COMMAND_BUF_SIZE            256

static volatile int keepRunning = 1;
uint8_t buf[BUF_SIZ];
int radiotap_length;

void intHandler(int dummy)
{
    keepRunning = 0;
}

/*struct ifreq findMACAdress(char interface[])
{
    int s;
    struct ifreq buffer;
    s = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&buffer, 0x00, sizeof(buffer));
    strcpy(buffer.ifr_name, interface);
    ioctl(s, SIOCGIFHWADDR, &buffer);
    close(s);
    return buffer;
}*/

int setBPF(int newsocket, uint8_t new_comm_id[4])
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
                    { 0x50,  0,  0, 0000000000 },
                    { 0x45,  1,  0, 0x00000008 }, // allow data frames
                    { 0x45,  0,  9, 0x00000080 }, // allow beacon frames
                    { 0x40,  0,  0, 0x00000006 },
                    { 0x15,  0,  7, 0x33445566 }, // comm_id 0xaabbccdd
                    { 0x48,  0,  0, 0x00000004 },
                    { 0x15,  0,  5, 0x00000101 }, // 0x<odd><direction>: 0x0101
                    { 0x48,  0,  0, 0x00000010 },
                    { 0x15,  0,  3, 0x00000101 }, // 0x<version><port> = 0x0101
                    { 0x50,  0,  0, 0x00000012 },
                    { 0x15,  0,  1, 0x00000001 }, // 0x<direction> = 0x01
                    { 0x06,  0,  0, 0x00040000 }, // accept and trim to 262144 bytes
                    { 0x06,  0,  0, 0000000000 },
            };

    // modify BPF Filter to fit the mac address of wifi card on drone (dst mac)
    uint32_t modded_mac_end = (new_comm_id[0]<<24) | (new_comm_id[1]<<16) | (new_comm_id[2]<<8) | new_comm_id[3];
    dest_filter[11].k = modded_mac_end;
    printf("DB_CONTROL_RX: BPF comm_ID: %02x\n", modded_mac_end);

    struct sock_fprog bpf =
            {
                    .len = ARRAY_SIZE(dest_filter),
                    .filter = dest_filter,
            };
    int ret = setsockopt(newsocket, SOL_SOCKET, SO_ATTACH_FILTER, &bpf, sizeof(bpf));
    if (ret < 0)
    {
        perror("DB_CONTROL_RX: could not attach BPF: ");
        close(newsocket);
        return -1;
    }
    return newsocket;
}

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
        perror("DB_CONTROL_RX: Unable to find interface index");
        exit(-1);
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
        perror("DB_CONTROL_RX: bind: ");
        exit(-1);
    }
    return newsocket;
}

int setUpNetworkIF(char newifName[IFNAMSIZ], char new_mode, uint8_t mac_drone[6])
{
    int sockfd, sockopt;
    //struct ifreq if_ip;	/* get ip addr */
    struct ifreq ifopts;	/* set promiscuous mode */
    //memset(&if_ip, 0, sizeof(struct ifreq));

    if (new_mode == 'w')
    {
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHER_TYPE))) == -1)
        {
            perror("DB_CONTROL_RX: error in wifi socket setup\n");
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
            perror("DB_CONTROL_RX: setsockopt");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1)
        {
            perror("DB_CONTROL_RX: error in monitor mode socket setup\n");
            return -1;
        }
        sockfd = setBPF(sockfd, mac_drone);
    }
    sockfd = bindsocket(sockfd, new_mode, newifName);
    return sockfd;
}

int packetisOK()
{
    // TODO may want to check crc header here
    return 1;
}

int determineRadiotapLength(int socket){
    printf("DB_CONTROL_RX: Waiting for first packet.\n");
    ssize_t length = recv(socket, buf, BUF_SIZ, 0);
    if (length < 0)
    {
        printf("DB_CONTROL_RX: Raw socket returned unrecoverable error: %s\n", strerror(errno));
        return 18; // might be true
    }
    radiotap_length = buf[2] | (buf[3] << 8);
    printf("DB_CONTROL_RX: Radiotapheader length is %i\n", radiotap_length);
    return radiotap_length;
}

int main(int argc, char *argv[])
{
    int sockfd, ret, i, c;
    int sizeetheheader = sizeof(struct ether_header);
    char ifName[IFNAMSIZ];
    char usbIF[IFNAMSIZ];
    char comm_id_Str[10];
    uint8_t comm_id[4];
    char ab_mode = 'm';
    //char interface[] = DEFAULT_IF;
// ------------------------------- Processing command line arguments ----------------------------------
    strncpy(ifName, DEFAULT_IF, IFNAMSIZ);
    strcpy(usbIF, USB_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:u:m:c:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(ifName, optarg, IFNAMSIZ);
                break;
            case 'u':
                strcpy(usbIF, optarg);
                break;
            case 'm':
                ab_mode = *optarg;
                break;
            case 'c':
                strncpy(comm_id_Str, optarg, 10);
                break;
            case '?':
                printf("Invalid commandline arguments. Use "
                               "\n-n <network_IF> "
                               "\n-u <USB_MSP_Interface_TO_FC>"
                               "\n-m [w|m] "
                               "\n-c <communication_id>");
                break;
            default:
                abort ();
        }
    }
    sscanf(comm_id_Str, "%2hhx%2hhx%2hhx%2hhx", &comm_id[0], &comm_id[1], &comm_id[2], &comm_id[3]);
    printf("DB_CONTROL_RX: Interface: %s Communication ID: %02x %02x %02x %02x\n", ifName, comm_id[0], comm_id[1], comm_id[2], comm_id[3]);
/*    struct ifreq buffer = findMACAdress(ifName);
    mac_drone[0] = (uint8_t)buffer.ifr_hwaddr.sa_data[0];
    mac_drone[1] = (uint8_t)buffer.ifr_hwaddr.sa_data[1];
    mac_drone[2] = (uint8_t)buffer.ifr_hwaddr.sa_data[2];
    mac_drone[3] = (uint8_t)buffer.ifr_hwaddr.sa_data[3];
    mac_drone[4] = (uint8_t)buffer.ifr_hwaddr.sa_data[4];
    mac_drone[5] = (uint8_t)buffer.ifr_hwaddr.sa_data[5];*/

// ------------------------------- Setting up Network Interface ----------------------------------

    /* Header structures */
    struct ether_header *eh = (struct ether_header *) buf;
    //struct iphdr *iph = (struct iphdr *) (buf + sizeof(struct ether_header));
    //struct udphdr *udph = (struct udphdr *) (buf + sizeof(struct iphdr) + sizeof(struct ether_header));
    sockfd = setUpNetworkIF(ifName, ab_mode, comm_id);

// ------------------------------- Setting up UART Interface ---------------------------------------
    int USB = -1;
    do
    {
        USB = open(usbIF, O_WRONLY | O_NOCTTY | O_NDELAY);
        if (USB == -1)
        {
            printf("DB_CONTROL_RX: Error - Unable to open UART.  Ensure it is not in use by another application and the FC is connected\n");
            printf("DB_CONTROL_RX: retrying ...\n");
            sleep(1);
        }
    }
    while(USB == -1);

    struct termios options;
    tcgetattr(USB, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(USB, TCIFLUSH);
    tcsetattr(USB, TCSANOW, &options);

// -------------------------------------- Loop ------------------------------------------------------
    int err, sentbytes;
    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    int command_length;

    radiotap_length = determineRadiotapLength(sockfd);
    printf("DB_CONTROL_RX: Starting MSP pass through!\n");
    while(keepRunning)
    {
        length = recv(sockfd, buf, BUF_SIZ, 0);
        err = errno;
        if (length < 0)
        {
            if (err == EAGAIN)
            {
            }
            else
            {
                printf("DB_CONTROL_RX: recvfrom returned unrecoverable error(errno=%d)\n", err);
                return -1;
            }
        }
        else
        {
            if(ab_mode == 'w')
            {
                // TODO
            }
            else
            {
                command_length = buf[radiotap_length+19] | (buf[radiotap_length+20] << 8);
                //printf("payload length: %i\n", command_length);
                memcpy(commandBuf, &buf[radiotap_length+AB80211_LENGTH], command_length);


            }
            //for (i=0; i<command_length; i++)
            //    printf("%02x:", commandBuf[i]);
            //printf("\n");

            sentbytes = (int) write(USB, commandBuf, command_length);
            tcdrain(USB);
            if (sentbytes > 0)
            {
                //printf(" and Sent!\n");
            }
            else if(sentbytes == 0)
            {
                printf(" NOT SENT!\n");
            }
            else
            {
                int errsv = errno;
                printf(" NOT SENT because of error %s\n", strerror(errsv));
            }
            tcflush(USB, TCIOFLUSH);
        }
    }

    close(sockfd);
    close(USB);
    printf("DB_CONTROL_RX: Sockets closed!\n");
    return 1;
}
