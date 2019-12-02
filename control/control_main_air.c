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
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <termios.h>    // POSIX terminal control definitionss
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <sys/time.h>
#include "../common/db_protocol.h"
#include "../common/db_raw_send_receive.h"
#include "../common/db_raw_receive.h"
#include "rc_air.h"
#include "../common/mavlink/c_library_v2/common/mavlink.h"
#include "../common/msp_serial.h"
#include "../common/db_utils.h"
#include "../common/radiotap/radiotap_iter.h"
#include "../common/db_common.h"


#define ETHER_TYPE        0x88ab
#define UART_IF          "/dev/serial1"
#define BUF_SIZ                      512    // should be enough?!
#define COMMAND_BUF_SIZE            1024
#define RETRANSMISSION_RATE            2    // send every MAVLink transparent packet twice for better reliability
#define DB_TRANSPARENT_READBUF         8    // bytes to read at once from serial port
#define STATUS_UPDATE_TIME    200    // send rc status to status module on groundstation every 200ms

static volatile int keep_running = 1;
uint8_t buf[BUF_SIZ];
uint8_t mavlink_telemetry_buf[2048] = {0}, mavlink_message_buf[256] = {0};
int mav_tel_message_counter = 0, mav_tel_buf_length = 0, cont_adhere_80211, num_inf = 0;
long double cpu_u_new[4], cpu_u_old[4], loadavg;
float systemp, millideg;

void intHandler(int dummy) {
    keep_running = 0;
}

speed_t interpret_baud(int user_baud) {
    switch (user_baud) {
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
 * Buffer 5 MAVLink telemetry messages before sending packet to ground station
 *
 * @param length_message Length of new MAVLink message
 * @param mav_message The pointer to a new MAVLink message
 * @param proxy_seq_number
 * @param raw_interfaces_telem
 */
void send_buffered_mavlink(int length_message, mavlink_message_t *mav_message, uint8_t *proxy_seq_number,
                           db_socket_t *raw_interfaces_telem) {
    mav_tel_message_counter++;  // Number of messages in buffer
    mavlink_msg_to_send_buffer(mavlink_message_buf, mav_message);   // Get over the wire representation of message
    // Copy message bytes into buffer
    memcpy(&mavlink_telemetry_buf[mav_tel_buf_length], mavlink_message_buf, (size_t) length_message);
    mav_tel_buf_length += length_message;   // Overall length of buffer
    if (mav_tel_message_counter == 5) {
        for (int i = 0; i < num_inf; i++) {
            db_send_div(&raw_interfaces_telem[i], mavlink_telemetry_buf, DB_PORT_PROXY,
                        (u_int16_t) mav_tel_buf_length, update_seq_num(proxy_seq_number),
                        cont_adhere_80211);
        }
        mav_tel_message_counter = 0;
        mav_tel_buf_length = 0;
    }
}

/**
 * Gets CPU usage on Linux systems. Needs to be called periodically. No one time calls!
 *
 * @return CPU load in %
 */
uint8_t get_cpu_usage() {
    FILE *fp;
    fp = fopen("/proc/stat", "r");
    if (fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &cpu_u_new[0], &cpu_u_new[1], &cpu_u_new[2], &cpu_u_new[3]) < 4)
        perror("DB_CONTROL_AIR: Could not read CPU usage");
    fclose(fp);
    loadavg = ((cpu_u_old[0] + cpu_u_old[1] + cpu_u_old[2]) - (cpu_u_new[0] + cpu_u_new[1] + cpu_u_new[2])) /
              ((cpu_u_old[0] + cpu_u_old[1] + cpu_u_old[2] + cpu_u_old[3]) -
               (cpu_u_new[0] + cpu_u_new[1] + cpu_u_new[2] + cpu_u_new[3])) * 100;
    memcpy(cpu_u_old, cpu_u_new, sizeof(cpu_u_new));
    return (uint8_t) loadavg;
}

/**
 * Reads the CPU temperature from a Linux system
 *
 * @return CPU temperature in °C
 */
uint8_t get_cpu_temp() {
    FILE *thermal;
    thermal = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (thermal == NULL || fscanf(thermal, "%f", &millideg) < 1)
        perror("DB_CONTROL_AIR: Could not read CPU temperature");
    else {
        fclose(thermal);
        systemp = millideg / 1000;
        return (uint8_t) systemp;
    }
    return 0;
}

/**
 * Send status update to status module
 *
 * @param status_seq_number
 * @param raw_interfaces_telem
 * @param rssi
 * @param start
 * @param start_rc
 * @param rc_packets_tmp
 * @param rc_packets_cnt
 * @param rc_status_update_data
 * @return
 */
uint8_t send_status_update(uint8_t *status_seq_number, db_socket_t *raw_interfaces_telem, int8_t rssi, long *start,
                           long *start_rc, uint8_t *rc_packets_tmp, uint8_t rc_packets_cnt,
                           struct uav_rc_status_update_message_t *rc_status_update_data, const long *rightnow) {
    struct timeval time_check;
    if ((*rightnow - *start_rc) >= 1000) {
        *rc_packets_tmp = rc_packets_cnt; // save received packets/seconds to temp variable
        rc_packets_cnt = 0;
        gettimeofday(&time_check, NULL);
        *start_rc = (long) time_check.tv_sec * 1000 + (long) time_check.tv_usec / 1000;
    }
    if ((*rightnow - *start) >= STATUS_UPDATE_TIME) {
        memset(rc_status_update_data, 0xff, 6);
        rc_status_update_data->rssi_rc_uav = rssi;
        rc_status_update_data->recv_pack_sec = *rc_packets_tmp;
        rc_status_update_data->cpu_usage_uav = get_cpu_usage();
        rc_status_update_data->cpu_temp_uav = get_cpu_temp();
        rc_status_update_data->uav_is_low_V = get_undervolt();
        for (int i = 0; i < num_inf; i++) {
            db_send_hp_div(&raw_interfaces_telem[i], DB_PORT_STATUS,
                           (u_int16_t) 14, update_seq_num(status_seq_number));
        }

        gettimeofday(&time_check, NULL);
        *start = (long) time_check.tv_sec * 1000 + (long) time_check.tv_usec / 1000;
    }
    return rc_packets_cnt;
}

/**
 * Open serial port for SUMD interface. Blocking call.
 *
 * @param sumd_interface Name of interface
 * @param socket_rc_serial
 * @return Socket to serial interface
 */
int open_serial_sumd(const char *sumd_interface) {
    int serial_socket;
    do {
        serial_socket = open(sumd_interface, O_WRONLY | O_NOCTTY | O_SYNC);
        if (serial_socket == -1) {
            LOG_SYS_STD(LOG_WARNING, "DB_CONTROL_AIR: Error - Unable to open UART for SUMD RC.  Ensure it is not "
                                     "in use by another application and the FC is connected. Retrying ... \n");
            sleep(1);
        }
    } while (serial_socket == -1);

    struct termios options_rc;
    tcgetattr(serial_socket, &options_rc);
    cfsetospeed(&options_rc, B115200);
    options_rc.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    options_rc.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
    options_rc.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    options_rc.c_cflag &= ~(CSIZE | PARENB);
    options_rc.c_cflag |= CS8;
    tcflush(serial_socket, TCIFLUSH);
    tcsetattr(serial_socket, TCSANOW, &options_rc);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_AIR: Opened SUMD interface on %s\n", sumd_interface);
    return serial_socket;
}

/**
 * Open serial port to receive telemetry. Non-blocking call
 *
 * @param baud_rate Baud rate of serial interface
 * @param telem_inf Name of serial interface connected with telemetry output
 * @return Socket to serial interface on success (>0) or -1 on failure
 */
int open_serial_telem(int baud_rate, const char *telem_inf) {
    int socket_control_serial;
    socket_control_serial = open(telem_inf, O_RDWR | O_NOCTTY | O_SYNC);
    if (socket_control_serial == -1) {
        LOG_SYS_STD(LOG_WARNING, "DB_CONTROL_AIR: Error - Unable to open UART for MSP/MAVLink.  Ensure it is not "
                                 "in use by another application and the FC is connected\n");
    } else {
        // configure serial socket
        struct termios options;
        tcgetattr(socket_control_serial, &options);
        options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
        options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
        options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
        options.c_cflag &= ~(CSIZE | PARENB);
        options.c_cflag |= CS8;
        cfsetispeed(&options, interpret_baud(baud_rate));
        cfsetospeed(&options, interpret_baud(baud_rate));

        options.c_cc[VMIN] = 1;            // wait for min. 1 byte (select trigger)
        options.c_cc[VTIME] = 0;           // timeout 0 second
        tcflush(socket_control_serial, TCIFLUSH);
        tcsetattr(socket_control_serial, TCSANOW, &options);
        LOG_SYS_STD(LOG_INFO, "DB_CONTROL_AIR: Opened telemetry serial port %s\n", telem_inf);
    }
    return socket_control_serial;
}

int main(int argc, char *argv[]) {
    int c, bitrate_op = 1, chucksize = 64;
    int serial_protocol_control = 2, baud_rate = 115200;
    char use_sumd = 'N';
    char sumd_interface[IFNAMSIZ];
    char telem_inf[IFNAMSIZ];
    uint8_t comm_id = DEFAULT_V2_COMMID, frame_type = DB_FRAMETYPE_DEFAULT;
    uint8_t status_seq_number = 0, proxy_seq_number = 0, serial_byte;
    char db_mode = 'm';
    char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];

// -------------------------------
// Processing command line arguments
// -------------------------------
    strcpy(telem_inf, UART_IF);
    strcpy(sumd_interface, UART_IF);
    cont_adhere_80211 = 0;
    opterr = 0;
    while ((c = getopt(argc, argv, "n:u:m:c:b:v:l:e:s:r:t:a:")) != -1) {
        switch (c) {
            case 'n':
                if (num_inf < DB_MAX_ADAPTERS) {
                    strncpy(adapters[num_inf], optarg, IFNAMSIZ);
                    num_inf++;
                }
                break;
            case 'u':
                strcpy(telem_inf, optarg);
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
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'r':
                baud_rate = (int) strtol(optarg, NULL, 10);
                break;
            case 't':
                frame_type = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'a':
                cont_adhere_80211 = (int) strtol(optarg, NULL, 10);
            case '?':
                printf("Invalid commandline arguments. Use "
                       "\n\t-n <Network interface name - multiple <-n interface> possible> "
                       "\n\t-u [MSP/MAVLink_Interface_TO_FC] - UART or VCP interface that is connected to FC"
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
                       "\n\t-r Baud rate of the serial interface -u (MSP/MAVLink) (2400, 4800, 9600, 19200, "
                       "38400, 57600, 115200 (default: %i))"
                       "\n\t-t [1|2] DroneBridge v2 raw protocol packet/frame type: 1=RTS, 2=DATA (CTS protection)\n"
                       "\n\t-b bit rate:\tin Mbps (1|2|5|6|9|11|12|18|24|36|48|54)\n\t\t(bitrate option only "
                       "supported with Ralink chipsets)"
                       "\n\t-a [0|1] to disable/enable. Offsets the payload by some bytes so that it sits outside "
                       "then 802.11 header. Set this to 1 if you are using a non DB-Rasp Kernel!",
                       chucksize, baud_rate);
                break;
            default:
                abort();
        }
    }
    conf_rc_serial_protocol_air(serial_protocol_control, use_sumd);
    open_rc_rx_shm(); // open/init shared memory to write RC values into it

// -------------------------------
// Setting up network interface
// -------------------------------
    db_socket_t raw_interfaces_rc[DB_MAX_ADAPTERS] = {0};
    db_socket_t raw_interfaces_telem[DB_MAX_ADAPTERS] = {0};
    for (int i = 0; i < num_inf; ++i) {
        raw_interfaces_rc[i] = open_db_socket(adapters[i], comm_id, db_mode, bitrate_op, DB_DIREC_GROUND, DB_PORT_RC,
                                              frame_type);
        raw_interfaces_telem[i] = open_db_socket(adapters[i], comm_id, db_mode, bitrate_op, DB_DIREC_GROUND,
                                                 DB_PORT_CONTROLLER, frame_type);
    }

// -------------------------------
// Setting up UART interface for MSP/MAVLink stream
// -------------------------------
    uint8_t transparent_buffer[chucksize];
    int socket_control_serial = open_serial_telem(baud_rate, telem_inf);
    int rc_serial_socket = socket_control_serial;

// -------------------------------
// Setting up UART interface for RC commands over SUMD
// -------------------------------
    if (use_sumd == 'Y') {
        rc_serial_socket = open_serial_sumd(sumd_interface);  // overwrite serial socket used for RC
    }

// ----------------------------------
// Loop
// ----------------------------------
    int sentbytes = 0, command_length = 0, errsv, select_return, continue_reading, chunck_left = chucksize,
            serial_read_bytes = 0, max_sd = 0;
    uint8_t serial_bytes[DB_TRANSPARENT_READBUF];
    int8_t rssi = -128, last_recv_rc_seq_num = 0, last_recv_cont_seq_num = 0;
    long start; // start time for status report update
    long start_rc; // start time for measuring the recv RC packets/second

    uint8_t rc_packets_tmp = 0, rc_packets_cnt = 0, seq_num_rc = 0, seq_num_cont = 0;
    mavlink_message_t mavlink_message;
    mavlink_status_t mavlink_status;
    mspPort_t db_msp_port;

    fd_set fd_socket_set;
    struct timeval socket_timeout;
    // wait max STATUS_UPDATE_TIME for message on socket
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = STATUS_UPDATE_TIME * 1000;

    ssize_t length;
    signal(SIGINT, intHandler);
    uint8_t commandBuf[COMMAND_BUF_SIZE];
    struct timeval timecheck;

    // create our data pointer directly inside the buffer (monitor_framebuffer) that is sent over the socket
    struct data_uni *raw_buffer = get_hp_raw_buffer(cont_adhere_80211);
    struct uav_rc_status_update_message_t *rc_status_update_data = (struct uav_rc_status_update_message_t *) raw_buffer;
    memset(raw_buffer->bytes, 0, DATA_UNI_LENGTH);

    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_AIR: Ready for data! Enabled diversity on %i adapters\n", num_inf);
    gettimeofday(&timecheck, NULL);
    start = (long) timecheck.tv_sec * 1000 + (long) timecheck.tv_usec / 1000; // [ms]
    long last_serial_telem_reconnect_try = start; // [ms]
    start_rc = start;
    uint16_t radiotap_lenght;
    while (keep_running) {
        socket_timeout.tv_sec = 0;
        socket_timeout.tv_usec = STATUS_UPDATE_TIME * 1000;
        FD_ZERO (&fd_socket_set);

        // add raw DroneBridge sockets
        for (int i = 0; i < num_inf; i++) {
            FD_SET (raw_interfaces_rc[i].db_socket, &fd_socket_set);
            if (raw_interfaces_rc[i].db_socket > max_sd)
                max_sd = raw_interfaces_rc[i].db_socket;
            if (raw_interfaces_telem[i].db_socket > max_sd)
                max_sd = raw_interfaces_telem[i].db_socket;
        }

        // Add or open serial interface for telemetry
        if (socket_control_serial > 0) {
            FD_SET (socket_control_serial, &fd_socket_set);
            if (socket_control_serial > max_sd)
                max_sd = socket_control_serial;
        }

        select_return = select(max_sd + 1, &fd_socket_set, NULL, NULL, &socket_timeout);

        if (select_return == -1 && errno != EINTR) {
            perror("DB_CONTROL_AIR: select returned error: ");
        } else if (select_return > 0) {
            // --------------------------------
            // DroneBridge long range data
            // --------------------------------
            for (int i = 0; i < num_inf; i++) {
                if (FD_ISSET(raw_interfaces_rc[i].db_socket, &fd_socket_set)) {
                    // --------------------------------
                    // DB_RC_PORT for DroneBridge RC packets
                    // --------------------------------
                    length = recv(raw_interfaces_rc[i].db_socket, buf, BUF_SIZ, 0);
                    if (length > 0) {
                        rc_packets_cnt++;
                        get_db_payload(buf, length, commandBuf, &seq_num_rc, &radiotap_lenght);
                        rssi = get_rssi(buf, radiotap_lenght);
                        if (last_recv_rc_seq_num != seq_num_rc) {  // diversity duplicate protection
                            last_recv_rc_seq_num = seq_num_rc;
                            command_length = generate_rc_serial_message(commandBuf);
                            if (command_length > 0 && rc_serial_socket > 0) {
                                sentbytes = (int) write(rc_serial_socket, serial_data_buffer, (size_t) command_length);
                                errsv = errno;
                                tcdrain(rc_serial_socket);
                                if (sentbytes <= 0) {
                                    LOG_SYS_STD(LOG_WARNING, "RC not written to serial interface %s\n",
                                                strerror(errsv));
                                }
                                // TODO: check if necessary. It shouldn't as we use blocking UART socket
                                // tcflush(rc_serial_socket, TCOFLUSH);
                            }
                        }
                    }
                }
            }

            for (int i = 0; i < num_inf; i++) {
                if (FD_ISSET(raw_interfaces_telem[i].db_socket, &fd_socket_set)) {
                    // --------------------------------
                    // DB_CONTROL_PORT for incoming MSP/MAVLink
                    // --------------------------------
                    length = recv(raw_interfaces_telem[i].db_socket, buf, BUF_SIZ, 0);
                    if (length > 0) {
                        rssi = get_rssi(buf, buf[2]);
                        if (last_recv_cont_seq_num != seq_num_cont) {  // diversity duplicate protection
                            last_recv_cont_seq_num = seq_num_cont;
                            command_length = get_db_payload(buf, length, commandBuf, &seq_num_cont, &radiotap_lenght);
                            if (socket_control_serial > 0) {
                                sentbytes = (int) write(socket_control_serial, commandBuf, (size_t) command_length);
                                errsv = errno;
                                tcdrain(socket_control_serial);
                                if (sentbytes < command_length) {
                                    LOG_SYS_STD(LOG_WARNING, "MSP/MAVLink NOT WRITTEN because of error: %s\n",
                                                strerror(errsv));
                                }
                                // TODO: check if necessary. It shouldn't as we use blocking UART socket
                                // tcflush(socket_control_serial, TCOFLUSH);
                            }
                        }
                    }
                }
            }
            // --------------------------------
            // FC input
            // --------------------------------
            if (FD_ISSET(socket_control_serial, &fd_socket_set)) {
                // --------------------------------
                // The FC sent us a MSP/MAVLink message - LTM telemetry will be ignored!
                // --------------------------------
                ssize_t read_bytes;
                switch (serial_protocol_control) {
                    default:
                    case 1:
                    case 2:
                        // Parse MSP message - just pass it to DB proxy module on ground station
                        continue_reading = 1;
                        serial_read_bytes = 0;
                        while (continue_reading) {
                            if (read(socket_control_serial, &serial_byte, 1) > 0) {
                                serial_read_bytes++;
                                // if MSP parser returns false stop reading from serial. We are reading shit or started
                                // reading during the middle of a message
                                if (mspSerialProcessReceivedData(&db_msp_port, serial_byte)) {
                                    raw_buffer->bytes[(serial_read_bytes - 1)] = serial_byte;
                                    if (db_msp_port.c_state == MSP_COMMAND_RECEIVED) {
                                        continue_reading = 0; // stop reading from serial port --> got a complete message!
                                        for (int i = 0; i < num_inf; i++) {
                                            db_send_hp_div(&raw_interfaces_telem[i], DB_PORT_PROXY,
                                                           (u_int16_t) serial_read_bytes,
                                                           update_seq_num(&proxy_seq_number));
                                        }
                                    }
                                } else {
                                    continue_reading = 0;
                                }
                            }/* else {
                                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                    close(socket_control_serial);
                                    socket_control_serial = -1;  // will try to reconnect in next loop iteration
                                    continue_reading = 0;
                                }
                            }*/
                        }
                        break;
                    case 3:
                    case 4:
                        // Parse complete MAVLink message
                        continue_reading = 1;
                        serial_read_bytes = 0;
                        while (continue_reading) {
                            if (read(socket_control_serial, &serial_byte, 1) > 0) {
                                serial_read_bytes++;
                                if (mavlink_parse_char(MAVLINK_COMM_0, (uint8_t) serial_byte, &mavlink_message,
                                                       &mavlink_status)) {
                                    continue_reading = 0; // stop reading from serial port --> got a complete message!
                                    mavlink_msg_to_send_buffer(raw_buffer->bytes, &mavlink_message);
                                    for (int i = 0; i < num_inf; i++) {
                                        db_send_hp_div(&raw_interfaces_telem[i], DB_PORT_PROXY,
                                                       (u_int16_t) chucksize, update_seq_num(&proxy_seq_number));
                                    }
                                }
                            }
                        }
                        break;
                    case 5:
                        // MAVLink plain pass through - no parsing. Send packets with length of chuck size
                        read_bytes = read(socket_control_serial, &serial_bytes, DB_TRANSPARENT_READBUF);
                        if (read_bytes > 0) {
                            memcpy(&transparent_buffer[serial_read_bytes], &serial_bytes, read_bytes);
                            serial_read_bytes += read_bytes;
                            if (serial_read_bytes >= chucksize) {
                                for (int i = 0; i < num_inf; i++) {
                                    LOG_SYS_STD(LOG_DEBUG, "DB_CONTROL_AIR: Sending transparent packet %i\n",
                                                serial_read_bytes);
                                    for (int r = 0; r < RETRANSMISSION_RATE; r++)
                                        db_send_div(&raw_interfaces_telem[i], transparent_buffer, DB_PORT_PROXY,
                                                    serial_read_bytes, update_seq_num(&proxy_seq_number),
                                                    cont_adhere_80211);
                                }
                                serial_read_bytes = 0;
                            }
                        }
                        break;
                }
            }
        }
        struct timeval time_check;
        gettimeofday(&time_check, NULL);
        long rightnow = (long) time_check.tv_sec * 1000 + (long) time_check.tv_usec / 1000;

        // --------------------------------
        // Send a status update to status module on ground station
        // --------------------------------
        rc_packets_cnt = send_status_update(&status_seq_number, raw_interfaces_telem, rssi, &start, &start_rc,
                                            &rc_packets_tmp, rc_packets_cnt, rc_status_update_data, &rightnow);
        // --------------------------------
        // Check for open telemetry serial socket
        // --------------------------------
        if (socket_control_serial < 0) {
            if ((rightnow - last_serial_telem_reconnect_try) >= 2000) { // try to open it every two seconds
                last_serial_telem_reconnect_try = rightnow;
                socket_control_serial = open_serial_telem(baud_rate, telem_inf);
                if (socket_control_serial > 0) {
                    if (use_sumd == 'N')  // do not overwrite SUMD if set
                        rc_serial_socket = socket_control_serial;
                }
            }
        }
    }

    for (int i = 0; i < DB_MAX_ADAPTERS; i++) {
        if (raw_interfaces_rc[i].db_socket > 0)
            close(raw_interfaces_rc[i].db_socket);
        if (raw_interfaces_telem[i].db_socket > 0)
            close(raw_interfaces_telem[i].db_socket);
    }
    close(socket_control_serial);
    close(rc_serial_socket);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_AIR: Terminated!\n");
    return 1;
}
