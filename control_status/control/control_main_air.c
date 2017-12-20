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
#include "../common/db_raw_send_receive.h"
#include "../common/db_raw_receive.h"
#include "rc_air.h"

#define ETHER_TYPE	    0x88ab
#define DEFAULT_IF      "18a6f716a511"
#define USB_IF          "/dev/ttyACM0"
#define BUF_SIZ		                512 // should be enought?!
#define COMMAND_BUF_SIZE            1024

static volatile int keepRunning = 1;
uint8_t buf[BUF_SIZ];

void intHandler(int dummy)
{
    keepRunning = 0;
}

int main(int argc, char *argv[])
{
    int c, chipset_type = 1, bitrate_op = 4;
    int rc_protocol = 2;
    char ifName[IFNAMSIZ];
    char usbIF[IFNAMSIZ];
    uint8_t comm_id = DEFAULT_V2_COMMID, status_seq_number = 0;
    char db_mode = 'm';

// -------------------------------
// Processing command line arguments
// -------------------------------
    strncpy(ifName, DEFAULT_IF, IFNAMSIZ);
    strcpy(usbIF, USB_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:u:m:c:a:b:v:")) != -1)
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
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'v':
                rc_protocol = (int) strtol(optarg, NULL, 10);
                break;
            case 'a':
                chipset_type = (int) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("Invalid commandline arguments. Use "
                               "\n\t-n <network_IF> "
                               "\n\t-u <USB_MSP/MAVLink_Interface_TO_FC> - set the baud rate to 115200 on your FC!"
                               "\n\t-m [w|m] (m = default)"
                               "\n\t-v Protocol over serial port [1|2]: 1 = MSPv1 [Betaflight/Cleanflight]; "
                               "2 = MSPv2 [iNAV] (default); 3 = MAVLink (unsupported); 4 = MAVLink v2 (unsupported)"
                               "\n\t-c <communication_id> Choose a number from 0-255. Same on groundstation and drone!"
                               "\n\t-a chipset type [1|2] <1> for Ralink und <2> for Atheros chipsets"
                               "\n\t-b bitrate: \n\t\t1 = 2.5Mbit\n\t\t2 = 4.5Mbit\n\t\t3 = 6Mbit\n\t\t4 = 12Mbit (default)\n\t\t"
                               "5 = 18Mbit\n\t\t(bitrate option only supported with Ralink chipsets)");
                break;
            default:
                abort ();
        }
    }
    conf_rc_serial_protocol_air(rc_protocol);
// -------------------------------
// Setting up Network Interface
// -------------------------------
    int socket_port_rc = open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_RC);
    //int socket_port_rc = open_receive_socket(ifName, db_mode, comm_id, DB_DIREC_DRONE, DB_PORT_RC);
    int socket_port_control = 0;
    //int socket_port_control = open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_GROUND, DB_PORT_CONTROLLER);
    //socket_port_rc = set_socket_timeout(socket_port_rc, 0, 25000);
    //socket_port_control = set_socket_nonblocking(socket_port_control);

// -------------------------------
//    Setting up UART Interface
// -------------------------------
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

// ----------------------------------
//       Loop
// ----------------------------------
    int err, sentbytes = 0, command_length = 0;
    int8_t rssi = -100;
    long start, rightnow, status_report_update_rate = 200; // send rc status to status module on groundstation every 200ms

    uint8_t packet_count = 0, packet_count_bad = 0;

    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    struct timeval timecheck;

    // create our data pointer directly inside the buffer (monitor_framebuffer) that is sent over the socket
    struct data_rc_status_update *rc_status_update_data = (struct data_rc_status_update *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);

    printf("DB_CONTROL_AIR: Starting MSP/MAVLink pass through!\n");
    gettimeofday(&timecheck, NULL);
    start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
    while(keepRunning)
    {
        // TODO: implement select() with these sockets! NO non blocking needed etc.
        // --------------------------------
        // DB_RC_PORT messages - has timeout - for DroneBridge RC packets
        // --------------------------------
        length = recv(socket_port_rc, buf, BUF_SIZ, 0); err = errno;
        if (length > 0){
            if (chipset_type == 1){
                rssi = buf[14];
            }else{
                rssi = buf[30];
            }
            memcpy(commandBuf, &buf[buf[2] + DB_RAW_V2_HEADER_LENGTH], (buf[buf[2]+7] | (buf[buf[2]+8] << 8)));
            command_length = generate_rc_serial_message(commandBuf);
            if (command_length > 0){
                packet_count++;
                sentbytes = (int) write(USB, serial_data_buffer, (size_t) command_length);
                tcdrain(USB);
                if(sentbytes == 0)
                {
                    printf(" RC NOT SENT!\n");
                }
                else
                {
                    int errsv = errno;
                    printf(" RC NOT SENT because of error %s\n", strerror(errsv));
                }
                tcflush(USB, TCIOFLUSH);
            } else {
                packet_count_bad++;
            }
        }

        // --------------------------------
        // DB_CONTROL_PORT messages - non blocking - for MSP/MAVLink
        // --------------------------------
        length = recv(socket_port_control, buf, BUF_SIZ, 0); err = errno;
        if (length <= 0)
        {
            if (err == EAGAIN)
            {
            }
            else
            {
                printf("DB_CONTROL_AIR: recv returned unrecoverable error %s)\n", strerror(err));
                return -1;
            }
        }
        else
        {
            if (chipset_type == 1){
                rssi = buf[14];
            }else{
                rssi = buf[30];
            }
            command_length = buf[buf[2]+7] | (buf[buf[2]+8] << 8); // ready for v2
            memcpy(commandBuf, &buf[buf[2] + DB_RAW_V2_HEADER_LENGTH], command_length);
            sentbytes = (int) write(USB, commandBuf, (size_t) command_length);
            tcdrain(USB);
            if(sentbytes == 0)
            {
                printf(" NOT SENT!\n");
            }
            else
            {
                int errsv = errno;
                printf(" NOT SENT because of error %s\n", strerror(errsv));
            }
            tcflush(USB, TCIOFLUSH);
            // TODO: check for response on UART and send to DroneBridge proxy module on groundstation; beware we might block RC!
        }

        // --------------------------------
        // Send a status update to status module on groundstation
        // --------------------------------
        gettimeofday(&timecheck, NULL);
        rightnow = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        if ((rightnow-start) >= status_report_update_rate){
            if (status_seq_number == 255){
                status_seq_number = 0;
            } else {
                status_seq_number++;
            }
            rc_status_update_data->bytes[0] = rssi;
            rc_status_update_data->bytes[1] = (int8_t) (packet_count * ((double) 1000 / (rightnow - start)));
            send_packet_hp( DB_PORT_STATUS, (u_int16_t) 2, status_seq_number);

            packet_count = 0; packet_count_bad = 0;
            gettimeofday(&timecheck, NULL);
            start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        }
    }

    close(socket_port_rc);
    close(socket_port_control);
    close(USB);
    printf("DB_CONTROL_AIR: Sockets closed!\n");
    return 1;
}
