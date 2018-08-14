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
#include <stdint.h>
#include <fcntl.h>
#include <zconf.h>
#include <termio.h>
#include <string.h>
#include <errno.h>
#include "../common/ccolors.h"
#include "../common/db_crc.h"
#include "../common/db_utils.h"


uint8_t serial_data_buffer[1024];

#define CRC_POLYNOME 0x1021
/******************************************
*************************************
* Function Name : CRC16
* Description : crc calculation, adds a 8 bit unsigned to 16 bit crc
*******************************************************************************/
uint16_t CRC16(uint16_t crc, uint8_t value)
{
    uint8_t i;
    crc = crc ^ (int16_t)value<<8;
    for(i=0; i<8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1) ^ CRC_POLYNOME;
        else
            crc = (crc << 1);
    }
    return crc;
}

void generate_sumd(uint16_t *rc_channel_data){
    for (int i = 0; i < 12; ++i) rc_channel_data[i] = (uint16_t) (rc_channel_data[i] * 8);
    serial_data_buffer[0] = 0xa8;
    serial_data_buffer[1] = 0x01;
    serial_data_buffer[2] = 0x0c;   // 12 channels
    // A
    serial_data_buffer[3] = (uint8_t) ((rc_channel_data[0] >> 8) & 0xFF);
    serial_data_buffer[4] = (uint8_t) (rc_channel_data[0] & 0xFF);
    // E
    serial_data_buffer[5] = (uint8_t) ((rc_channel_data[1] >> 8) & 0xFF);
    serial_data_buffer[6] = (uint8_t) (rc_channel_data[1] & 0xFF);
    // T
    serial_data_buffer[7] = (uint8_t) ((rc_channel_data[2] >> 8) & 0xFF);
    serial_data_buffer[8] = (uint8_t) (rc_channel_data[2] & 0xFF);
    // R
    serial_data_buffer[9] = (uint8_t) ((rc_channel_data[3] >> 8) & 0xFF);
    serial_data_buffer[10] = (uint8_t) (rc_channel_data[3] & 0xFF);
    // 5
    serial_data_buffer[11] = (uint8_t) ((rc_channel_data[4] >> 8) & 0xFF);
    serial_data_buffer[12] = (uint8_t) (rc_channel_data[4] & 0xFF);
    // 6
    serial_data_buffer[13] = (uint8_t) ((rc_channel_data[5] >> 8) & 0xFF);
    serial_data_buffer[14] = (uint8_t) (rc_channel_data[5] & 0xFF);
    // 7
    serial_data_buffer[15] = (uint8_t) ((rc_channel_data[6] >> 8) & 0xFF);
    serial_data_buffer[16] = (uint8_t) (rc_channel_data[6] & 0xFF);
    // 8
    serial_data_buffer[17] = (uint8_t) ((rc_channel_data[7] >> 8) & 0xFF);
    serial_data_buffer[18] = (uint8_t) (rc_channel_data[7] & 0xFF);
    // 9
    serial_data_buffer[19] = (uint8_t) ((rc_channel_data[8] >> 8) & 0xFF);
    serial_data_buffer[20] = (uint8_t) (rc_channel_data[8] & 0xFF);
    // 10
    serial_data_buffer[21] = (uint8_t) ((rc_channel_data[9] >> 8) & 0xFF);
    serial_data_buffer[22] = (uint8_t) (rc_channel_data[9] & 0xFF);
    // 11
    serial_data_buffer[23] = (uint8_t) ((rc_channel_data[10] >> 8) & 0xFF);
    serial_data_buffer[24] = (uint8_t) (rc_channel_data[10] & 0xFF);
    // 12
    serial_data_buffer[25] = (uint8_t) ((rc_channel_data[11] >> 8) & 0xFF);
    serial_data_buffer[26] = (uint8_t) (rc_channel_data[11] & 0xFF);
    // crc
    uint16_t crc_sumd_air = 0;
    for (int i_sumd_air = 0; i_sumd_air < 27; i_sumd_air++) {
        crc_sumd_air = (crc_sumd_air << 8) ^ crc_sumd_table[(crc_sumd_air >> 8) ^ serial_data_buffer[i_sumd_air]];
    }
    serial_data_buffer[27] = (uint8_t) ((crc_sumd_air >> 8) & 0xFF);
    serial_data_buffer[28] = (uint8_t) (crc_sumd_air & 0xFF);
    crc_sumd_air = 0;
}

int main(int argc, char *argv[]) {
    uint16_t my_channel_data[] = {1500, 1500, 1500, 1900, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
    generate_sumd(my_channel_data);
    int socket_rc_serial = -1, rc_serial_socket;
    do
    {
        socket_rc_serial = open("/dev/ttyUSB0", O_WRONLY | O_NOCTTY | O_SYNC);
        if (socket_rc_serial == -1)
        {
            printf(RED "DB_CONTROL_AIR: Error - Unable to open UART for SUMD RC.  Ensure it is not in use by another "
                   "application and the FC is connected. Retrying"RESET"\n");
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

    for (int i = 0; i < 200; ++i) {
        int sentbytes = (int) write(rc_serial_socket, serial_data_buffer, (size_t) 29); int errsv = errno;
        tcdrain(rc_serial_socket);
        if(sentbytes < 29)
        {
            printf(" RC NOT WRITTEN because of error: %s\n", strerror(errsv));
        }
        //tcflush(rc_serial_socket, TCOFLUSH);
        //printf("Wrote SUMD to FC\n");
        print_buffer(serial_data_buffer, 29);

        usleep(10000);
    }
    close(rc_serial_socket);
}