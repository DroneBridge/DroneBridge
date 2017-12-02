//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <termios.h>    // POSIX terminal control definitionss
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include "../common/db_protocol.h"
#include <sys/time.h>
#include "../common/db_raw_send.h"
#include "../common/db_raw_receive.h"

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

int determineRadiotapLength(int socket){
    printf("DB_CONTROL_AIR: Waiting for first packet.\n");
    ssize_t length = recv(socket, buf, BUF_SIZ, 0);
    if (length < 0)
    {
        printf("DB_CONTROL_AIR: Raw socket returned unrecoverable error: %s\n", strerror(errno));
        return 18; // might be true
    }
    radiotap_length = buf[2] | (buf[3] << 8);
    printf("DB_CONTROL_AIR: Radiotapheader length is %i\n", radiotap_length);
    return radiotap_length;
}

int main(int argc, char *argv[])
{
    int c, frame_type = 1, bitrate_op = 4;
    int size_ether_header = sizeof(struct ether_header);
    char ifName[IFNAMSIZ];
    char usbIF[IFNAMSIZ];
    char comm_id_Str[10];
    uint8_t comm_id[4];
    char db_mode = 'm';

// ------------------------------- Processing command line arguments ----------------------------------
    strncpy(ifName, DEFAULT_IF, IFNAMSIZ);
    strcpy(usbIF, USB_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:u:m:c:a:b:")) != -1)
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
                db_mode = *optarg;
                break;
            case 'c':
                strncpy(comm_id_Str, optarg, 10);
                break;
            case 'a':
                frame_type = (int) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("Invalid commandline arguments. Use "
                               "\n-n <network_IF> "
                               "\n-u <USB_MSP/MAVLink_Interface_TO_FC> - set the baud rate to 115200 on your FC!"
                               "\n-m [w|m] "
                               "\n-c <communication_id>"
                               "\n-a frame type [1|2] <1> for Ralink und <2> for Atheros chipsets"
                               "\n-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n(bitrate option only supported with Ralink chipsets)");
                break;
            default:
                abort ();
        }
    }
    sscanf(comm_id_Str, "%2hhx%2hhx%2hhx%2hhx", &comm_id[0], &comm_id[1], &comm_id[2], &comm_id[3]);
    printf("DB_CONTROL_AIR: Interface: %s Communication ID: %02x %02x %02x %02x\n", ifName, comm_id[0], comm_id[1],
           comm_id[2], comm_id[3]);

// ------------------------------- Setting up Network Interface ----------------------------------

    /* Header structures */
    //struct ether_header *eh = (struct ether_header *) buf;
    //struct iphdr *iph = (struct iphdr *) (buf + sizeof(struct ether_header));
    //struct udphdr *udph = (struct udphdr *) (buf + sizeof(struct iphdr) + sizeof(struct ether_header));
    //socket_receive = setUpNetworkIF(ifName, db_mode, comm_id);
    int socket_receive = open_receive_socket(ifName, db_mode, comm_id, DB_DIREC_DRONE, DB_PORT_CONTROLLER);
    open_socket_sending(ifName,comm_id, db_mode, bitrate_op, frame_type, DB_DIREC_GROUND);

// ------------------------------- Setting up UART Interface ---------------------------------------
    int USB = -1;
    do
    {
        USB = open(usbIF, O_WRONLY | O_NOCTTY | O_NDELAY);
        if (USB == -1)
        {
            printf("DB_CONTROL_AIR: Error - Unable to open UART.  Ensure it is not in use by another application and the"
                           " FC is connected\n");
            printf("DB_CONTROL_AIR: retrying ...\n");
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
    int8_t rssi = -100;
    uint8_t packet_count = 0;
    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    int command_length = 0;
    long start, end, status_report_update_rate = 200; // send rc status to status module on groundstation every 200ms
    struct timeval timecheck;

    // create our data pointer directly inside the buffer (monitor_framebuffer_uni) that is sent over the socket
    struct data_rc_status_update *rc_status_update_data = (struct data_rc_status_update *)
            (monitor_framebuffer_uni + RADIOTAP_LENGTH + DB80211_HEADER_LENGTH);

    radiotap_length = determineRadiotapLength(socket_receive);
    socket_receive = set_socket_timeout(socket_receive, 0, 250000);
    printf("DB_CONTROL_AIR: Starting MSP/MAVLink pass through!\n");
    while(keepRunning)
    {
        gettimeofday(&timecheck, NULL);
        start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

        length = recv(socket_receive, buf, BUF_SIZ, 0);
        err = errno;
        if (length <= 0)
        {
            if (err == EAGAIN)
            {
            }
            else
            {
                printf("DB_CONTROL_AIR: recv returned unrecoverable error(errno=%d)\n", err);
                return -1;
            }
        }
        else
        {
            packet_count++;
            if(db_mode == 'w')
            {
                // TODO implement wifi mode
            }
            else
            {
                command_length = buf[radiotap_length+19] | (buf[radiotap_length+20] << 8);
                memcpy(commandBuf, &buf[radiotap_length + DB80211_HEADER_LENGTH], command_length);
                if (frame_type == 1){
                    rssi = buf[14];
                }else{
                    rssi = buf[30];
                }
            }
            //for (i=0; i<command_length; i++)
            //    printf("%02x:", commandBuf[i]);
            //printf("\n");

            sentbytes = (int) write(USB, commandBuf, (size_t) command_length);
            tcdrain(USB);
            if (sentbytes > 0)
            {

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
            // TODO: check for response on UART and send to DroneBridge proxy module on groundstation
        }

        gettimeofday(&timecheck, NULL);
        end = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        if ((end-start) >= status_report_update_rate){
            rc_status_update_data->bytes[0] = rssi;
            rc_status_update_data->bytes[1] = packet_count;
            send_packet_hp( DB_PORT_STATUS, (u_int16_t) 2);
            packet_count = 0;
        }
    }

    close(socket_receive);
    close_socket_send();
    close(USB);
    printf("DB_CONTROL_AIR: Sockets closed!\n");
    return 1;
}
