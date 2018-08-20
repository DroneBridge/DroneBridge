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

#include "telemetry_main_air.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zconf.h>
#include <memory.h>
#include <net/if.h>
#include <getopt.h>
#include <stdbool.h>
#include <signal.h>
#include <termios.h>
#include <termio.h>
#include <fcntl.h>
#include "../common/db_protocol.h"
#include "../common/ccolors.h"
#include "../common/db_utils.h"
#include "../common/db_raw_send_receive.h"

bool volatile keep_running = true;
char if_name_telemetry[IFNAMSIZ], if_name_serial[IFNAMSIZ], telem_type[15] = "auto";
char db_mode;
char serial_port[] = "/dev/ttyAMA0";
uint8_t comm_id = DEFAULT_V2_COMMID, tel_seq_number = 0;
int c, baud_rate, serial_socket, buffer_two_ltm_messages, bitrate_op;
uint8_t ltm_frame_buf[MAX_LTM_FRAME_SIZE*2];


void intHandler(int dummy)
{
    keep_running = false;
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

void setup_serial_port(){
    do
    {
        serial_socket = open(if_name_serial, O_RDONLY | O_NOCTTY | O_SYNC);
        if (serial_socket == -1)
        {
            printf(YEL "DB_TELEMETRY_AIR: Error - Unable to open UART for telemetry stream. Ensure it is not in use by "
                   "another application and the FC is connected. Retrying ..."RESET"\n");
            sleep(1);
        }
    }
    while(serial_socket == -1);
    // Set up serial port
    struct termios options;
    tcgetattr(serial_socket, &options);
    options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
    options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    options.c_cflag &= ~(CSIZE | PARENB);
    options.c_cflag |= CS8;
    cfsetispeed(&options, interpret_baud(baud_rate));
    cfsetospeed(&options, interpret_baud(baud_rate));
    options.c_cc[VMIN]  = 1;            // wait for min. 1 byte (select trigger)
    options.c_cc[VTIME] = 10;           // timeout 1 second
    tcflush(serial_socket, TCIFLUSH);
    if (tcsetattr(serial_socket, TCSANOW, &options) != 0)
        perror("DB_TELEMETRY_AIR: Could not set serial socket options");
}

/**
 * Read a specified number of bytes form the @var serial_socket and stores it inside the provided buffer
 * @param num_bytes Number of bytes to read form serial_socket
 * @param a_buffer Pointer to buffer to store data in
 */
void read_bytes_from_serial(size_t num_bytes, uint8_t a_buffer[])
{
    ssize_t size_recv;
    int num_bytes_received = 0;

    while(1)
    {
        size_recv = read(serial_socket , &a_buffer[num_bytes_received], num_bytes-num_bytes_received);
        if(size_recv > 0)
        {
            num_bytes_received += size_recv;
            if (num_bytes == num_bytes_received) break;
        }
        else
        {
            perror(RED "DB_TELEMETRY_AIR: Reading from serial port (buffered read)"RESET"\n");
        }
    }
    // printf("Read: %i bytes form serial port!\n", num_bytes_received);
}

/**
 * Call after first two bytes of LTM frame have already been read! This function expects the function byte as next byte
 * on the serial line!
 * Read the rest of a LTM frame and stores a complete frame inside global variable ltm_frame
 * @param pos_ltm_frame_buffer Position in the ltm_frame buffer where we want to store the newly read frame
 * @return The size of the entire LTM frame that was read
 */
int read_remaining_ltm_frame(int pos_ltm_frame_buffer) {
    uint8_t function_byte;
    int frame_size = 0;
    if (read(serial_socket, &function_byte, 1) > 0) {
        switch (function_byte){
            case 'A':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TA", 3);
                read_bytes_from_serial(LTM_SIZE_ATT, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_ATT+3;
                break;
            case 'S':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TS", 3);
                read_bytes_from_serial(LTM_SIZE_STATUS, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_STATUS+3;
                break;
            case 'G':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TG", 3);
                read_bytes_from_serial(LTM_SIZE_GPS, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_GPS+3;
                break;
            case 'O':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TO", 3);
                read_bytes_from_serial(LTM_SIZE_GPS, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_GPS+3;
                break;
            case 'N':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TN", 3);
                read_bytes_from_serial(LTM_SIZE_ATT, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_ATT+3;
                break;
            case 'X':
                memcpy(&ltm_frame_buf[pos_ltm_frame_buffer], "$TX", 3);
                read_bytes_from_serial(LTM_SIZE_ATT, &ltm_frame_buf[pos_ltm_frame_buffer+3]);
                frame_size = LTM_SIZE_ATT+3;
                break;
            default:
                printf(RED "Unknown LTM frame!" RESET "\n");
                break;
        }
    }
    return frame_size;
}

/**
 * Checks the crc of a frame stored inside @var ltm_frame with the length @param frame_size
 * @param frame_size The complete size of the LTM frame to check
 * @return true if crc of LTM frame is valid
 */
bool check_ltm_crc(int frame_size) {
    uint8_t crc = 0x00;
    for (int i = 3; i < frame_size; i++) {
        crc ^= ltm_frame_buf[i];
    }
    return crc == 0;
}

/**
 * Checks first 50 bytes of serial interface if they contain a LTM frame. Checks if ltm crc is valid.
 * @return 0 if LTM detected, else 1 for any other protocol
 */
int detect_telemetry_type() {
    uint8_t serial_byte;
    int i = 0;
    printf("DB_TELEMETRY_AIR: Detecting telemetry stream type...\n");
    for(i; i < 50; i++){
        ssize_t read_size = read(serial_socket, &serial_byte, 1);
        if (read_size > 0) {
            if (serial_byte == '$'){
                if (read(serial_socket, &serial_byte, 1) > 0) {
                    if (serial_byte == 'T'){
                        if (check_ltm_crc(read_remaining_ltm_frame(0))){
                            printf(GRN"DB_TELEMETRY_AIR: Detected LTM telemetry stream"RESET"\n");
                            return 0;
                        }
                    }
                } else {
                    perror("DB_TELEMETRY_AIR: detection");
                }
            }
        } else {
            perror("DB_TELEMETRY_AIR: detection");
        }
    }
    printf(GRN"DB_TELEMETRY_AIR: Detected MAVLink telemetry or unknown stream"RESET"\n");
    return 1;
}

int process_command_line_args(int argc, char *argv[]){
    strncpy(if_name_telemetry, DEFAULT_DB_IF, IFNAMSIZ);
    strncpy(if_name_serial, serial_port, IFNAMSIZ);
    db_mode = DEFAULT_DB_MODE;
    //baud_rate = 4800;
    baud_rate = 115200;
    buffer_two_ltm_messages = 1;
    bitrate_op = 4;
    opterr = 0;
    while ((c = getopt (argc, argv, "n:f:l:m:r:c:b:x:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(if_name_telemetry, optarg, IFNAMSIZ);
                break;
            case 'f':
                strncpy(if_name_serial, optarg, IFNAMSIZ);
                break;
            case 'l':
                strncpy(telem_type, optarg, 15);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'r':
                baud_rate = (int) strtol(optarg, NULL, 10);
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'x':
                buffer_two_ltm_messages = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("usage: telemetry_air [-n DB_INTERFACE] [-f SERIALPORT]\n"
                       "                           [-l TELEMETRY_TYPE] [-m MODE]\n"
                       "                           [-r BAUDRATE] [-c COMM_ID] -x [BUFFER_LTM]\n"
                       "                           [-b BITRATE]\n"
                       "\n"
                       "Put this file on your drone. It handles telemetry only.\n"
                       "\n"
                       "optional arguments:\n"
                       "  -n DB_INTERFACE    Network interface on which we send out packets to drone.\n"
                       "                     Should be the interface for long-range comm (default: wlan1)\n"
                       "  -f SERIALPORT      Serial port which is connected to flight controller and\n"
                       "                     receives the telemetry (default: /dev/ttyAMA0)\n"
                       "  -l TELEMETRY_TYPE  Set telemetry type manually. Default is [auto]. Use\n"
                       "                     [ltm|mavlink|auto]\n"
                       "  -x BUFFER_LTM      [0=No|1=Yes] Always buffer two ltm messages to send them together\n"
                       "  -b BITRATE         Transmission bit rate for the long range link. (Ralink chipsets only)\n"
                       "                     \t1 = 2.5Mbit\n"
                       "                     \t2 = 4.5Mbit\n"
                       "                     \t3 = 6Mbit\n"
                       "                     \t4 = 12Mbit (default)\n"
                       "                     \t5 = 18Mbit\n"
                       "  -m MODE            Set the mode in which communication should happen. Use\n"
                       "                     [wifi|monitor]\n"
                       "  -r BAUDRATE        Baudrate for the serial port:\n"
                       "                     [115200|57600|38400|19200|9600|4800|2400]\n"
                       "  -c COMM_ID         Communication ID must be the same on drone and\n"
                       "                     ground station. A number between 0-255 Example: \"125\"\n"
                       "");
                break;
            default:
                abort ();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);

    process_command_line_args(argc, argv);

    uint8_t serial_byte;
    uint8_t transparent_buffer[transparent_chunksize];
    ssize_t read_bytes;
    int ltm_frame_buffer_pos = 0;

    setup_serial_port();
    db_socket tel_db_socket = open_db_socket(if_name_telemetry, comm_id, db_mode, bitrate_op, DB_DIREC_GROUND,
            DB_PORT_TELEMETRY);

    int fixed_telem_type = 1; // 0=LTM, 1=MAVLink/pass through
    int ltm_frame_size = 0;
    if (strncmp(telem_type, "auto", 4) == 0){
        fixed_telem_type = detect_telemetry_type();
    } else if (strncmp(telem_type, "ltm", 3) == 0){
        fixed_telem_type = 0;
    }

    while(keep_running)
    {
        if (fixed_telem_type == 0){
            // parsed telemetry downstream for LTM
            if (read(serial_socket, &serial_byte, 1) > 0) {
                if (serial_byte == '$'){
                    if (read(serial_socket, &serial_byte, 1) > 0) {
                        if (serial_byte == 'T'){
                            if (buffer_two_ltm_messages){
                                // Always buffer two LTM messages and send them together
                                ltm_frame_size += read_remaining_ltm_frame(ltm_frame_buffer_pos);
                                if (ltm_frame_buffer_pos>0) {
                                    send_packet_div(&tel_db_socket, ltm_frame_buf, DB_PORT_TELEMETRY,
                                                    (u_int16_t) ltm_frame_size, update_seq_num(&tel_seq_number));
                                    //print_buffer(ltm_frame_buf, ltm_frame_size);
                                    ltm_frame_buffer_pos=0;
                                    ltm_frame_size = 0;
                                } else {
                                    ltm_frame_buffer_pos=ltm_frame_size;
                                }
                            } else {
                                // Send one LTM message per packet over long range link
                                ltm_frame_size = read_remaining_ltm_frame(0);
                                send_packet_div(&tel_db_socket, ltm_frame_buf, DB_PORT_TELEMETRY,
                                                (u_int16_t) ltm_frame_size, update_seq_num(&tel_seq_number));
                                //print_buffer(ltm_frame_buf, ltm_frame_size);
                            }
                        }
                    }

                }
            } else {
                perror("DB_TELEMETRY_AIR: Reading from serial port (LTM)");
            }
        } else {
            // fully transparent telemetry downstream for non-LTM stream
            read_bytes_from_serial(transparent_chunksize, transparent_buffer);
            send_packet_div(&tel_db_socket, transparent_buffer, DB_PORT_TELEMETRY,
                            (u_int16_t) transparent_chunksize, update_seq_num(&tel_seq_number));
        }
    }
    close(serial_socket);
}
