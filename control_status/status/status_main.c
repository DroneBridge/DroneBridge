/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *   parts based on rx_status by Rodizio. Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
 *   integrated into the DroneBridge extensions by seeul8er
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zconf.h>
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include "../common/lib.h"
#include "../common/db_get_ip.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/shared_memory.h"

bool volatile keeprunning = true;
char if_name[IFNAMSIZ];
char db_mode;
uint8_t comm_id;
int c, comm_id_num = DEFAULT_V2_COMMID, app_port_status = APP_PORT_STATUS;
// TODO: get RC send rate from control module
float rc_send_rate = 60; // [packets/s]

void intHandler(int dummy)
{
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]){
    strncpy(if_name, DEFAULT_DB_IF, IFNAMSIZ);
    db_mode = DEFAULT_DB_MODE;
    app_port_status = APP_PORT_STATUS;
    opterr = 0;
    while ((c = getopt (argc, argv, "n:m:c:p:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(if_name, optarg, IFNAMSIZ);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'c':
                comm_id_num = (int) strtol(optarg, NULL, 10);
                break;
            case 'p':
                app_port_status = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("This tool sends extra information about the video stream and RC via UDP to IP given by "
                               "IP-checker module. Use"
                               "\n\t-n <network_IF> "
                               "\n\t-m [w|m] default is <m>"
                               "\n\t-p Specify a UDP port to which we send the status information. IP comes from IP "
                               "checker module. Default:%i"
                               "\n\t-c <communication id> Choose a number from 0-255. Same on groundstation and drone!"
                        , APP_PORT_STATUS);
                break;
            default:
                abort ();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    int restarts = 0, udp_socket, shID, best_dbm = 0, cardcounter = 0, err, radiotap_length;
    ssize_t l;
    uint8_t counter = 0;
    uint8_t lr_buffer[DATA_UNI_LENGTH];

    DB_RC_STATUS_MESSAGE db_rc_status_message;
    db_rc_status_message.ident[0] = '$';
    db_rc_status_message.ident[1] = 'D';
    db_rc_status_message.message_id = 2;

    DB_SYSTEM_STATUS_MESSAGE db_sys_status_message;
    db_sys_status_message.ident[0] = '$';
    db_sys_status_message.ident[1] = 'D';
    db_sys_status_message.message_id = 1;
    db_sys_status_message.mode = 'm';
    db_sys_status_message.packetloss_rc = 0;
    db_sys_status_message.rssi_drone = 0;
    db_sys_status_message.crc = 0x66;
    comm_id = (uint8_t) comm_id_num;

    process_command_line_args(argc, argv);

    // open db rc shared memory
    db_rc_values *rc_values = db_rc_values_memory_open();
    // open wbc rx status shared memory
    wifibroadcast_rx_status_t *wbc_rx_status = wbc_status_memory_open();
    int number_cards = wbc_rx_status->wifi_adapter_cnt;

    // set up long range receiving socket
    int long_range_socket = open_receive_socket(if_name, db_mode, comm_id, DB_DIREC_GROUND, DB_PORT_STATUS);
    long_range_socket = set_socket_nonblocking(long_range_socket);

    // set up UDP socket
    struct sockaddr_in remoteServAddr;
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_addr.s_addr = inet_addr("192.168.2.2");
    remoteServAddr.sin_port = htons(app_port_status);
    udp_socket = socket (AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        printf ("DB_STATUS_GROUND: %s: Unable to open socket\n", strerror(errno));
        exit (EXIT_FAILURE);
    }
    int broadcast=1;
    if (setsockopt(udp_socket,SOL_SOCKET,SO_BROADCAST, &broadcast,sizeof(broadcast))==-1) {
        printf("DB_STATUS_GROUND: %s\n",strerror(errno));
    }

    // get IP shared memory ID
    shID = init_shared_memory_ip();

    printf("DB_STATUS_GROUND: started!\n");
    while(keeprunning) {
        // ---------------
        // Get IP from IP Checker shared memory segment 10th time
        // ---------------
        counter++;
        if (counter>9){
            counter = 0;
            remoteServAddr.sin_addr.s_addr = inet_addr(get_ip_from_ipchecker(shID));
        }

        // ---------------
        // status message
        // ---------------
        l = recv(long_range_socket, lr_buffer, DATA_UNI_LENGTH, 0);
        if (l > 0){
            // get payload
            radiotap_length = lr_buffer[2] | (lr_buffer[3] << 8);
            // message_length = lr_buffer[radiotap_length+19] | (lr_buffer[radiotap_length+20] << 8);
            // process payload (currently only one type of raw status frame is supported: RC_AIR --> STATUS_GROUND)
            // must be a data_rc_status_update
            db_sys_status_message.rssi_drone = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH];
            db_sys_status_message.packetloss_rc = (uint8_t) (
                    (1 - (lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 1] / rc_send_rate)) * 100);
        }
        best_dbm = -1000;
        for(cardcounter=0; cardcounter<number_cards; ++cardcounter) {
            if (best_dbm < wbc_rx_status->adapter[cardcounter].current_signal_dbm)
                best_dbm = wbc_rx_status->adapter[cardcounter].current_signal_dbm;
        }
        db_sys_status_message.rssi_ground = best_dbm;
        db_sys_status_message.damaged_blocks_wbc = wbc_rx_status->damaged_block_cnt;
        db_sys_status_message.lost_packets_wbc = wbc_rx_status->lost_packet_cnt;
        db_sys_status_message.kbitrate_wbc = wbc_rx_status-> kbitrate;
        if (wbc_rx_status->tx_restart_cnt > restarts) {
            restarts++;
            usleep ((__useconds_t) 1e7);
        }
        sendto (udp_socket, &db_sys_status_message, sizeof(DB_SYSTEM_STATUS_MESSAGE), 0, (struct sockaddr *) &remoteServAddr,
                sizeof (remoteServAddr));

        // ---------------
        // RC message
        // ---------------
        memcpy(db_rc_status_message.channels, rc_values->ch, 2*NUM_CHANNELS);
        sendto (udp_socket, &db_rc_status_message, sizeof(DB_RC_STATUS_MESSAGE), 0, (struct sockaddr *) &remoteServAddr,
                sizeof (remoteServAddr));
        // DEBUG
        //printf( "%c[;H", 27 );
        //printf("\nRC values form control module: %i %i %i %i %i %i %i", rc_values->ch[0], rc_values->ch[1],
        // rc_values->ch[2], rc_values->ch[3], rc_values->ch[4], rc_values->ch[5], rc_values->ch[6]);



        usleep((__useconds_t) 100000); // 10Hz
    }
    close(udp_socket);
    return 0;
}