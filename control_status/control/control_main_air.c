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
#include "../common/mavlink_v2/common/mavlink.h"



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

speed_t interpret_baud(int user_baud){
    switch (user_baud){
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        default:
            return B115200;
    }
}

int main(int argc, char *argv[])
{
    //TODO: Integrate telemetry module! Multi core option: We launch an extra thread to do telemetry
    int c, chipset_type = 1, bitrate_op = 4;
    int serial_protocol_control = 2, baud_rate = 115200;
    char ifName[IFNAMSIZ];
    char usbIF[IFNAMSIZ];
    uint8_t comm_id = DEFAULT_V2_COMMID;
    uint8_t status_seq_number = 0, proxy_seq_number = 0, telemetry_seq_number = 0, serial_byte;
    char db_mode = 'm';

// -------------------------------
// Processing command line arguments
// -------------------------------
    strncpy(ifName, DEFAULT_IF, IFNAMSIZ);
    strcpy(usbIF, USB_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:u:m:c:a:b:v:r:")) != -1)
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
                serial_protocol_control = (int) strtol(optarg, NULL, 10);
                break;
            case 'a':
                chipset_type = (int) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'r':
                baud_rate = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("Invalid commandline arguments. Use "
                               "\n\t-n <network_IF> "
                               "\n\t-u <USB_MSP/MAVLink_Interface_TO_FC> - set the baud rate to 115200 on your FC!"
                               "\n\t-m [w|m] (m = default)"
                               "\n\t-v Protocol over serial port [1|2|3|4]: 1 = MSPv1 [Betaflight/Cleanflight]; "
                               "2 = MSPv2 [iNAV] (default); 3 = MAVLink (RC unsupported); 4 = MAVLink v2 (RC unsupported)"
                               "\n\t-c <communication_id> Choose a number from 0-255. Same on groundstation and drone!"
                               "\n\t-a chipset type [1|2] <1> for Ralink und <2> for Atheros chipsets"
                               "\n\t-r Baud rate of the serial interface (2400, 4800, 9600, 19200, 38400, 57600, 115200 (default))"
                               "\n\t-b bit rate: \n\t\t1 = 2.5Mbit\n\t\t2 = 4.5Mbit\n\t\t3 = 6Mbit\n\t\t4 = 12Mbit (default)\n\t\t"
                               "5 = 18Mbit\n\t\t(bitrate option only supported with Ralink chipsets)");
                break;
            default:
                abort ();
        }
    }
    conf_rc_serial_protocol_air(serial_protocol_control);
// -------------------------------
// Setting up Network Interface
// -------------------------------
    //int socket_port_rc = open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_RC);
    int socket_port_rc = open_receive_socket(ifName, db_mode, comm_id, DB_DIREC_DRONE, DB_PORT_RC);
    int socket_port_control = open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_GROUND, DB_PORT_CONTROLLER);

// -------------------------------
//    Setting up UART Interface
// -------------------------------
    int socket_control_serial = -1;
    do
    {
        socket_control_serial = open(usbIF, O_RDWR | O_NOCTTY);
        if (socket_control_serial == -1)
        {
            printf("DB_CONTROL_AIR: Error - Unable to open UART.  Ensure it is not in use by another application and the"
                           " FC is connected\n");
            printf("DB_CONTROL_AIR: retrying ...\n");
            sleep(1);
        }
    }
    while(socket_control_serial == -1);

    struct termios options;
    tcgetattr(socket_control_serial, &options);
    options.c_cflag = interpret_baud(baud_rate) | CS8 | CLOCAL;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    options.c_cc[VMIN]  = 1;            // wait for min. 1 byte (select trigger)
    options.c_cc[VTIME] = 0;           // timeout 0 second
    tcflush(socket_control_serial, TCIFLUSH);
    tcsetattr(socket_control_serial, TCSANOW, &options);

// ----------------------------------
//       Loop
// ----------------------------------
    int sentbytes = 0, command_length = 0, errsv, select_return, continue_reading, err_serial, serial_read_bytes;
    int8_t rssi = 0;
    long start, rightnow, status_report_update_rate = 200; // send rc status to status module on groundstation every 200ms

    uint8_t lost_packet_count = 0, last_seq_numer = 0;
    mavlink_message_t mavlink_message;
    mavlink_status_t mavlink_status;

    fd_set fd_socket_set;
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = status_report_update_rate*1000; // wait max status_report_update_rate for message on socket

    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    struct timeval timecheck;

    // create our data pointer directly inside the buffer (monitor_framebuffer) that is sent over the socket
    struct data_rc_status_update *rc_status_update_data = (struct data_rc_status_update *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
    struct data_uni *data_uni_to_ground = (struct data_uni *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
    memset(data_uni_to_ground->bytes, 0, DATA_UNI_LENGTH);

    printf("DB_CONTROL_AIR: Starting MSP/MAVLink pass through!\n");
    gettimeofday(&timecheck, NULL);
    start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
    while(keepRunning)
    {
        FD_ZERO (&fd_socket_set);
        FD_SET (socket_port_rc, &fd_socket_set);
        FD_SET (socket_port_control, &fd_socket_set);
        FD_SET (socket_control_serial, &fd_socket_set);
        select_return = select (FD_SETSIZE, &fd_socket_set, NULL, NULL, &socket_timeout);

        if(select_return == -1)
        {
            perror("DB_CONTROL_AIR: select returned error: ");
        }else if (select_return > 0){
            if (FD_ISSET(socket_port_rc, &fd_socket_set)){
                // --------------------------------
                // DB_RC_PORT for DroneBridge RC packets
                // --------------------------------
                length = recv(socket_port_rc, buf, BUF_SIZ, 0);
                if (length > 0){
                    if (chipset_type == 1){
                        rssi = buf[14];
                    }else{
                        rssi = buf[30];
                    }
                    memcpy(commandBuf, &buf[buf[2] + DB_RAW_V2_HEADER_LENGTH], (buf[buf[2]+7] | (buf[buf[2]+8] << 8)));
                    command_length = generate_rc_serial_message(commandBuf);
                    if (command_length > 0){
                        lost_packet_count += count_lost_packets(last_seq_numer, buf[buf[2]+9]);
                        last_seq_numer = buf[buf[2]+9];
                        sentbytes = (int) write(socket_control_serial, serial_data_buffer, (size_t) command_length); errsv = errno;
                        tcdrain(socket_control_serial);
                        if(sentbytes <= 0)
                        {
                            printf(" RC NOT WRITTEN because of error: %s\n", strerror(errsv));
                        }
                        // TODO: check if necessary. It shouldn't as we use blocking UART socket
                        tcflush(socket_control_serial, TCOFLUSH);
                    }
                }
            }
            if (FD_ISSET(socket_port_control, &fd_socket_set)){
                // --------------------------------
                // DB_CONTROL_PORT for MSP/MAVLink
                // --------------------------------
                length = recv(socket_port_control, buf, BUF_SIZ, 0);
                if (length <= 0)
                {
                    if (chipset_type == 1){
                        rssi = buf[14];
                    }else{
                        rssi = buf[30];
                    }
                    command_length = buf[buf[2]+7] | (buf[buf[2]+8] << 8); // ready for v2
                    memcpy(commandBuf, &buf[buf[2] + DB_RAW_V2_HEADER_LENGTH], command_length);
                    sentbytes = (int) write(socket_control_serial, commandBuf, (size_t) command_length); errsv = errno;
                    tcdrain(socket_control_serial);
                    if(sentbytes <= 0)
                    {
                        printf(" MSP/MAVLink NOT WRITTEN because of error: %s\n", strerror(errsv));
                    }
                    // TODO: check if necessary. It shouldn't as we use blocking UART socket
                    tcflush(socket_control_serial, TCOFLUSH);
                }
            }
            if (FD_ISSET(socket_control_serial, &fd_socket_set)){
                // --------------------------------
                // The FC sent us a MSP/MAVLink message - no LTM telemetry here!
                // --------------------------------
                switch (serial_protocol_control){
                    case 1:
                    case 2:
                        // Parse MSP message - just pass it to DB Proxy module on groundstation
                        break;
                    case 3:
                    case 4:
                        continue_reading = 1;
                        serial_read_bytes = 0;
                        while(continue_reading){
                            // Parse complete MAVLink message - telemetry --> DB Telemetry module; other --> DB Proxy module
                            if (read(socket_control_serial, &serial_byte, 1) > 0) {
                                serial_read_bytes++;
                                if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t) serial_byte, &mavlink_message,
                                                       &mavlink_status)) {
                                    continue_reading = 0; // stop reading from serial port --> got a complete message!
                                    switch (mavlink_message.msgid) {
                                        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
                                        case MAVLINK_MSG_ID_SYS_STATUS:
                                        case MAVLINK_MSG_ID_RC_CHANNELS:
                                        case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
                                        case MAVLINK_MSG_ID_GPS_RAW_INT:
                                        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
                                        case MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN:
                                        case MAVLINK_MSG_ID_ATTITUDE:
                                        case MAVLINK_MSG_ID_VFR_HUD:
                                        case MAVLINK_MSG_ID_HEARTBEAT:
                                        case MAVLINK_MSG_ID_MISSION_CURRENT:
                                            // TODO: send to telemetry module
                                            memcpy(data_uni_to_ground->bytes, &mavlink_message,
                                                   (size_t) serial_read_bytes);
                                            send_packet_hp(DB_PORT_TELEMETRY, (u_int16_t) serial_read_bytes,
                                                           update_seq_num(&telemetry_seq_number));
                                            break;
                                        default:
                                            // TODO: send to proxy module
                                            memcpy(data_uni_to_ground->bytes, &mavlink_message,
                                                   (size_t) serial_read_bytes);
                                            send_packet_hp(DB_PORT_PROXY, (u_int16_t) serial_read_bytes,
                                                           update_seq_num(&proxy_seq_number));
                                            break;
                                    }
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        // --------------------------------
        // Send a status update to status module on groundstation
        // --------------------------------
        gettimeofday(&timecheck, NULL);
        rightnow = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        if ((rightnow-start) >= status_report_update_rate){
            memset(rc_status_update_data->bytes, 0xff, 6);
            rc_status_update_data->bytes[0] = rssi;
            // lost packets/second (it is a estimate)
            rc_status_update_data->bytes[1] = (int8_t) (lost_packet_count * ((double) 1000 / (rightnow - start)));
            send_packet_hp( DB_PORT_STATUS, (u_int16_t) 6, update_seq_num(&status_seq_number));

            lost_packet_count = 0;
            gettimeofday(&timecheck, NULL);
            start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        }
    }

    close(socket_port_rc);
    close(socket_port_control);
    close(socket_control_serial);
    printf("DB_CONTROL_AIR: Sockets closed!\n");
    return 1;
}
