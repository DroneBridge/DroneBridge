/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2018 Wolfgang Christl
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
#include "../common/mavlink/c_library_v2/common/mavlink.h"
#include "../common/msp_serial.h"
#include "../common/ccolors.h"
#include "../common/db_utils.h"


#define ETHER_TYPE	    0x88ab
#define DEFAULT_IF      "wlx000ee8dcaa2c"
#define USB_IF          "/dev/ttyACM0"
#define BUF_SIZ		                512 // should be enought?!
#define COMMAND_BUF_SIZE            1024

static volatile int keepRunning = 1;
uint8_t buf[BUF_SIZ];
uint8_t mavlink_telemetry_buf[2048] = {0}, mavlink_message_buf[256] = {0};
uint8_t telemetry_seq_number = 0;
int mav_tel_message_counter = 0, mav_tel_buf_length = 0;
long double cpu_u_new[4], cpu_u_old[4], loadavg;
float systemp, millideg;

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

/**
 * Buffer 5 MAVLink telemetry messages before sending packet to groundstation
 * @param length_message Length of new MAVLink message
 * @param mav_message The pointer to a new MAVLink message
 */
void send_buffered_mavlink_tel(int length_message, mavlink_message_t *mav_message) {
    mav_tel_message_counter++;  // Number of messages in buffer
    mavlink_msg_to_send_buffer(mavlink_message_buf, mav_message);   // Get over the wire representation of message
    // Copy message bytes into buffer
    memcpy(&mavlink_telemetry_buf[mav_tel_buf_length], mavlink_message_buf, (size_t) length_message);
    mav_tel_buf_length += length_message;   // Overall length of buffer
    if (mav_tel_message_counter == 5){
        send_packet(mavlink_telemetry_buf, DB_PORT_TELEMETRY,
                    (u_int16_t) mav_tel_buf_length, update_seq_num(&telemetry_seq_number));
        mav_tel_message_counter = 0;
        mav_tel_buf_length = 0;
    }
}

/**
 * Gets CPU usage on Linux systems. Needs to be called periodically. No one time calls!
 * @return CPU load in %
 */
uint8_t get_cpu_usage(){
    FILE *fp;
    fp = fopen("/proc/stat","r");
    if (fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &cpu_u_new[0], &cpu_u_new[1], &cpu_u_new[2], &cpu_u_new[3]) < 4)
        perror("DB_CONTROL_AIR: Could not read CPU usage\n");
    fclose(fp);
    loadavg = ((cpu_u_old[0] + cpu_u_old[1] + cpu_u_old[2]) - (cpu_u_new[0] + cpu_u_new[1] + cpu_u_new[2])) /
              ((cpu_u_old[0]+cpu_u_old[1]+cpu_u_old[2]+cpu_u_old[3]) - (cpu_u_new[0]+cpu_u_new[1]+cpu_u_new[2]+cpu_u_new[3]))*100;
    memcpy(cpu_u_old, cpu_u_new, sizeof(cpu_u_new));
    return (uint8_t) loadavg;
}

/**
 * Reads the CPU temperature from a Linux system
 * @return CPU temperature in °C
 */
uint8_t get_cpu_temp(){
    FILE *thermal;
    thermal = fopen("/sys/class/thermal/thermal_zone0/temp","r");
    if (fscanf(thermal,"%f",&millideg) < 1)
        perror("DB_CONTROL_AIR: Could not read CPU temperature\n");
    fclose(thermal);
    systemp = millideg / 1000;
    return (uint8_t) systemp;
}

int main(int argc, char *argv[])
{
    int c, chipset_type = 1, bitrate_op = 4, chucksize = 64;
    int serial_protocol_control = 2, baud_rate = 115200;
    char use_sumd = 'N';
    char sumd_interface[IFNAMSIZ];
    char ifName[IFNAMSIZ];
    char usbIF[IFNAMSIZ];
    uint8_t comm_id = DEFAULT_V2_COMMID;
    uint8_t status_seq_number = 0, proxy_seq_number = 0, serial_byte;
    char db_mode = 'm';

// -------------------------------
// Processing command line arguments
// -------------------------------
    strncpy(ifName, DEFAULT_IF, IFNAMSIZ);
    strcpy(usbIF, USB_IF);
    strcpy(sumd_interface, USB_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:u:m:c:a:b:v:l:e:s:r:")) != -1)
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
            case 'l':
                chucksize = (int) strtol(optarg, NULL, 10);
                break;
            case 'e':
                use_sumd = *optarg;
                break;
            case 's':
                strcpy(sumd_interface, optarg);
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
                               "\n\t-u <USB_MSP/MAVLink_Interface_TO_FC> - UART or USB interface that is connected to FC"
                               "\n\t-m [w|m] (m = default, w = unsupported) DroneBridge mode - wifi/monitor"
                               "\n\t-v Protocol over serial port [1|2|3|4]:\n"
                                  "\t\t1 = MSPv1 [Betaflight/Cleanflight]\n"
                                  "\t\t2 = MSPv2 [iNAV] (default)\n"
                                  "\t\t3 = MAVLink v1 (RC unsupported)\n"
                                  "\t\t4 = MAVLink v2\n"
                                  "\t\t5 = MAVLink (plain) pass through (-l <chunk size>) - recommended with MAVLink, "
                                  "FC needs to support MAVLink v2 for RC"
                               "\n\t-l only relevant with -v 5 option. Telemetry bytes per packet over long range "
                               "(default: %i)"
                               "\n\t-e [Y|N] enable/disable RC over SUMD. If disabled -v & -u options are used for RC."
                               "\n\t-s Specify a serial port for use with SUMD. Ignored if SUMD is deactivated. Must be "
                               "different from one specified with -u"
                               "\n\t-c <communication_id> Choose a number from 0-255. Same on groundstation and drone!"
                               "\n\t-a chipset type [1|2] <1> for Ralink und <2> for Atheros chipsets"
                               "\n\t-r Baud rate of the serial interface -u (MSP/MAVLink) (2400, 4800, 9600, 19200, "
                               "38400, 57600, 115200 (default: %i))"
                               "\n\t-b bit rate: \n\t\t1 = 2.5Mbit\n\t\t2 = 4.5Mbit\n\t\t3 = 6Mbit"
                               "\n\t\t4 = 12Mbit (default)\n\t\t5 = 18Mbit\n\t\t(bitrate option only supported with "
                               "Ralink chipsets)", chucksize, baud_rate);
                break;
            default:
                abort ();
        }
    }
    conf_rc_serial_protocol_air(serial_protocol_control, use_sumd);
    open_rc_rx_shm(); // open/init shared memory to write RC values into it
// -------------------------------
// Setting up network interface
// -------------------------------
    int socket_port_rc = open_receive_socket(ifName, db_mode, comm_id, DB_DIREC_DRONE, DB_PORT_RC);
    int socket_port_control = open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_GROUND, DB_PORT_CONTROLLER);

// -------------------------------
//    Setting up UART interface for MSP/MAVLink stream
// -------------------------------
    uint8_t serial_bytes_buffer[chucksize];
    uint8_t transparent_buffer[chucksize];
    int socket_control_serial = -1;
    do
    {
        socket_control_serial = open(usbIF, O_RDWR | O_NOCTTY | O_SYNC);
        if (socket_control_serial == -1)
        {
            printf("DB_CONTROL_AIR: Error - Unable to open UART for MSP/MAVLink.  Ensure it is not in use by another "
                           "application and the FC is connected\n");
            printf("DB_CONTROL_AIR: retrying ...\n");
            sleep(1);
        }
    }
    while(socket_control_serial == -1);

    struct termios options;
    tcgetattr(socket_control_serial, &options);
    options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8;
    cfsetispeed(&options, interpret_baud(baud_rate));
    cfsetospeed(&options, interpret_baud(baud_rate));

    options.c_cc[VMIN]  = 1;            // wait for min. 1 byte (select trigger)
    options.c_cc[VTIME] = 0;           // timeout 0 second
    tcflush(socket_control_serial, TCIFLUSH);
    tcsetattr(socket_control_serial, TCSANOW, &options);
    int rc_serial_socket = socket_control_serial;

// -------------------------------
//    Setting up UART interface for RC commands over SUMD
// -------------------------------
    int socket_rc_serial = -1;
    if (use_sumd == 'Y'){
        do
        {
            socket_rc_serial = open(sumd_interface, O_WRONLY | O_NOCTTY | O_SYNC);
            if (socket_rc_serial == -1)
            {
                printf(RED "DB_CONTROL_AIR: Error - Unable to open UART for SUMD RC.  Ensure it is not in use by another"
                               " application and the FC is connected. Retrying ... "RESET"\n");
                sleep(1);
            }
        }
        while(socket_rc_serial == -1);

        struct termios options_rc;
        tcgetattr(socket_rc_serial, &options_rc);
        cfsetospeed(&options_rc, B115200);
        options_rc.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
        options_rc.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
        options_rc.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
        options_rc.c_cflag &= ~(CSIZE | PARENB);
        options_rc.c_cflag |= CS8;
        tcflush(socket_rc_serial, TCIFLUSH);
        tcsetattr(socket_rc_serial, TCSANOW, &options_rc);
        rc_serial_socket = socket_rc_serial;
    }

// ----------------------------------
//       Loop
// ----------------------------------
    int sentbytes = 0, command_length = 0, errsv, select_return, continue_reading, chunck_left = chucksize, serial_read_bytes = 0;
    int8_t rssi = 0;
    long start, rightnow, status_report_update_rate = 200; // send rc status to status module on groundstation every 200ms

    uint8_t lost_packet_count = 0, last_seq_numer = 0;
    mavlink_message_t mavlink_message;
    mavlink_status_t mavlink_status;
    mspPort_t db_msp_port;

    fd_set fd_socket_set;
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = status_report_update_rate*1000; // wait max status_report_update_rate for message on socket

    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    struct timeval timecheck;

    // create our data pointer directly inside the buffer (monitor_framebuffer) that is sent over the socket
    struct uav_rc_status_update_message *rc_status_update_data = (struct uav_rc_status_update_message *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
    struct data_uni *data_uni_to_ground = (struct data_uni *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
    memset(data_uni_to_ground->bytes, 0, DATA_UNI_LENGTH);

    printf("DB_CONTROL_AIR: Ready for data!\n");
    gettimeofday(&timecheck, NULL);
    start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
    while(keepRunning)
    {
        socket_timeout.tv_sec = 0;
        socket_timeout.tv_usec = status_report_update_rate*1000;
        FD_ZERO (&fd_socket_set);
        FD_SET (socket_port_rc, &fd_socket_set);
        FD_SET (socket_port_control, &fd_socket_set);
        FD_SET (socket_control_serial, &fd_socket_set);
        select_return = select (FD_SETSIZE, &fd_socket_set, NULL, NULL, &socket_timeout);

        if(select_return == -1)
        {
            perror("DB_CONTROL_AIR: select() returned error: ");
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
                        sentbytes = (int) write(rc_serial_socket, serial_data_buffer, (size_t) command_length); errsv = errno;
                        tcdrain(rc_serial_socket);
                        if(sentbytes <= 0)
                        {
                            printf(RED "RC NOT WRITTEN because of error: %s"RESET"\n", strerror(errsv));
                        }
                        // TODO: check if necessary. It shouldn't as we use blocking UART socket
                        // tcflush(rc_serial_socket, TCOFLUSH);
                    }
                }
            }
            if (FD_ISSET(socket_port_control, &fd_socket_set)){
                // --------------------------------
                // DB_CONTROL_PORT for MSP/MAVLink
                // --------------------------------
                length = recv(socket_port_control, buf, BUF_SIZ, 0);
                if (length > 0)
                {
                    if (chipset_type == 1){
                        rssi = buf[14];
                    }else{
                        rssi = buf[30];
                    }
                    command_length = buf[buf[2] + 7] | (buf[buf[2] + 8] <<  8); // ready for v2
                    memcpy(commandBuf, &buf[buf[2] + DB_RAW_V2_HEADER_LENGTH], (size_t) command_length);
                    sentbytes = (int) write(socket_control_serial, commandBuf, (size_t) command_length); errsv = errno;
                    tcdrain(socket_control_serial);
                    if(sentbytes < command_length)
                    {
                        printf(RED"MSP/MAVLink NOT WRITTEN because of error: %s"RESET"\n", strerror(errsv));
                    }
                    // TODO: check if necessary. It shouldn't as we use blocking UART socket
                    tcflush(socket_control_serial, TCOFLUSH);
                }
            }
            if (FD_ISSET(socket_control_serial, &fd_socket_set)){
                // --------------------------------
                // The FC sent us a MSP/MAVLink message - LTM telemetry will be ignored!
                // --------------------------------
                switch (serial_protocol_control){
                    default:
                    case 1:
                    case 2:
                        // Parse MSP message - just pass it to DB Proxy module on groundstation
                        continue_reading = 1;
                        serial_read_bytes = 0;
                        while (continue_reading){
                            if (read(socket_control_serial, &serial_byte, 1) > 0) {
                                serial_read_bytes++;
                                // if MSP parser returns false stop reading from serial. We are reading shit or started
                                // reading during the middle of a message
                                if (mspSerialProcessReceivedData(&db_msp_port, serial_byte)){
                                    data_uni_to_ground->bytes[(serial_read_bytes-1)] = serial_byte;
                                    if (db_msp_port.c_state == MSP_COMMAND_RECEIVED){
                                        continue_reading = 0; // stop reading from serial port --> got a complete message!
                                        send_packet_hp(DB_PORT_PROXY, (u_int16_t) serial_read_bytes,
                                                       update_seq_num(&proxy_seq_number));
                                    }
                                } else {
                                    continue_reading = 0;
                                }
                            }
                        }
                        break;
                    case 3:
                    case 4:
                        // Parse complete MAVLink message - telemetry --> DB Telemetry module; other --> DB Proxy module
                        continue_reading = 1;
                        serial_read_bytes = 0;
                        while (continue_reading){
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
                                            send_buffered_mavlink_tel(serial_read_bytes, &mavlink_message);
                                            break;
                                        default:
                                            mavlink_msg_to_send_buffer(data_uni_to_ground->bytes, &mavlink_message);
                                            send_packet_hp(DB_PORT_PROXY, (u_int16_t) serial_read_bytes,
                                                           update_seq_num(&proxy_seq_number));
                                            break;
                                    }
                                }
                            }
                        }
                        break;
                    case 5:
                        // MAVLink plain pass through - no parsing. Send packets with length of chuck size to tel module
                        if (read(socket_control_serial, &serial_byte, 1) > 0) {
                            transparent_buffer[serial_read_bytes] = serial_byte;
                            serial_read_bytes++;
                            if (serial_read_bytes == chucksize) {
                                send_packet(transparent_buffer, DB_PORT_TELEMETRY,
                                            (u_int16_t) chucksize, update_seq_num(&telemetry_seq_number));
                                serial_read_bytes = 0;
                            }
                        }
                        break;
                }
            }
        }

        // --------------------------------
        // Send a status update to status module on ground station
        // --------------------------------
        gettimeofday(&timecheck, NULL);
        rightnow = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        if ((rightnow-start) >= status_report_update_rate){
            memset(rc_status_update_data->bytes, 0xff, 6);
            rc_status_update_data->bytes[0] = rssi;
            // lost packets/second (it is a estimate)
            rc_status_update_data->bytes[1] = (int8_t) (lost_packet_count * ((double) 1000 / (rightnow - start)));
            rc_status_update_data->bytes[2] = get_cpu_usage();
            rc_status_update_data->bytes[3] = get_cpu_temp();
            rc_status_update_data->bytes[4] = get_undervolt();
            send_packet_hp(DB_PORT_STATUS, (u_int16_t) 6, update_seq_num(&status_seq_number));

            lost_packet_count = 0;
            gettimeofday(&timecheck, NULL);
            start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
        }
    }

    close(socket_port_rc);
    close(socket_port_control);
    close(socket_control_serial);
    close(rc_serial_socket);
    printf("DB_CONTROL_AIR: Sockets closed!\n");
    return 1;
}
