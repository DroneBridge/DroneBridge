/*
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

bool volatile keeprunning = true;
char if_name[IFNAMSIZ];
char db_mode;
uint8_t comm_id;
int c, comm_id_num = DEFAULT_V2_COMMID;
// TODO: get RC send rate from control module
float rc_send_rate = 60; // [packets/s]

void intHandler(int dummy)
{
    keeprunning = false;
}

wifibroadcast_rx_status_t *status_memory_open() {
    int fd;
    for(;;) {
        fd = shm_open("/wifibroadcast_rx_status_0", O_RDWR, S_IRUSR | S_IWUSR);
        if(fd > 0) {
            break;
        }
        printf("DB_STATUS_GROUND: Waiting for groundstation video to be started ...\n");
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(wifibroadcast_rx_status_t)) == -1) {
        perror("DB_STATUS_GROUND: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(wifibroadcast_rx_status_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("DB_STATUS_GROUND: mmap");
        exit(1);
    }
    return (wifibroadcast_rx_status_t*)retval;
}

int process_command_line_args(int argc, char *argv[]){
    strncpy(if_name, DEFAULT_DB_IF, IFNAMSIZ);
    db_mode = DEFAULT_DB_MODE;
    opterr = 0;
    while ((c = getopt (argc, argv, "n:m:c:")) != -1)
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
            case '?':
                printf("This tool sends extra information about the video stream and RC via UDP to port %i. Use "
                               "\n-n <network_IF> "
                               "\n-m [w|m] "
                               "\n-c <communication_id> choose 0-255", APP_PORT_STATUS);
                break;
            default:
                abort ();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    int restarts = 0, fd, shID, best_dbm = 0, cardcounter = 0, err, radiotap_length, message_length;
    ssize_t l;
    uint8_t counter = 0;
    uint8_t lr_buffer[DATA_UNI_LENGTH];
    struct DB_STATUS_FRAME db_status_frame;
    db_status_frame.ident[0] = '$';
    db_status_frame.ident[1] = 'D';
    db_status_frame.message_id = 1;
    db_status_frame.mode = 'm';
    db_status_frame.packetloss_rc = 0;
    db_status_frame.rssi_drone = 0;
    db_status_frame.crc = 0x66;
    comm_id = (uint8_t) comm_id_num;

    process_command_line_args(argc, argv);
    // open wbc rx status shared memory
    wifibroadcast_rx_status_t *wbc_rx_status = status_memory_open();
    int number_cards = wbc_rx_status->wifi_adapter_cnt;

    // set up long range receiving socket
    int long_range_socket = open_receive_socket(if_name, db_mode, comm_id, DB_DIREC_GROUND, DB_PORT_STATUS);
    long_range_socket = set_socket_nonblocking(long_range_socket);

    // set up UDP socket
    struct sockaddr_in remoteServAddr;
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_addr.s_addr = inet_addr("192.168.2.2");
    remoteServAddr.sin_port = htons(APP_PORT_STATUS);
    fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf ("DB_STATUS_GROUND: %s: Unable to open socket \n", strerror(errno));
        exit (EXIT_FAILURE);
    }
    int broadcast=1;
    if (setsockopt(fd,SOL_SOCKET,SO_BROADCAST, &broadcast,sizeof(broadcast))==-1) {
        printf("DB_STATUS_GROUND: %s",strerror(errno));
    }

    // get IP shared memory ID
    shID = init_shared_memory_ip();

    printf("DB_STATUS_GROUND: started!");
    while(keeprunning) {
        counter++;
        // Get IP from IP Checker shared memory segment 10th time
        if (counter>9){
            counter = 0;
            remoteServAddr.sin_addr.s_addr = inet_addr(get_ip_from_ipchecker(shID));
        }
        // Check for incoming data
        l = recv(long_range_socket, lr_buffer, DATA_UNI_LENGTH, 0);
        err = errno;
        if (l>0){
            // get payload
            radiotap_length = lr_buffer[2] | (lr_buffer[3] << 8);
            // message_length = lr_buffer[radiotap_length+19] | (lr_buffer[radiotap_length+20] << 8);
            // process payload (currently only one type of raw status frame is supported: RC_AIR --> STATUS_GROUND)
            // must be a data_rc_status_update
            db_status_frame.rssi_drone = lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH];
            db_status_frame.packetloss_rc = (uint8_t) (
                    (1 - (lr_buffer[radiotap_length + DB_RAW_V2_HEADER_LENGTH + 1] / rc_send_rate)) * 100);
        }

        best_dbm = -1000;
        for(cardcounter=0; cardcounter<number_cards; ++cardcounter) {
            if (best_dbm < wbc_rx_status->adapter[cardcounter].current_signal_dbm)
                best_dbm = wbc_rx_status->adapter[cardcounter].current_signal_dbm;
        }
        db_status_frame.rssi_ground = best_dbm;
        db_status_frame.damaged_blocks_wbc = htonl(wbc_rx_status->damaged_block_cnt);
        db_status_frame.lost_packets_wbc = htonl(wbc_rx_status->lost_packet_cnt);
        db_status_frame.kbitrate_wbc = htonl(wbc_rx_status-> kbitrate);
        if (wbc_rx_status->tx_restart_cnt > restarts) {
            restarts++;
            usleep ((__useconds_t) 1e7);
        }

        sendto (fd, &db_status_frame, sizeof(struct DB_STATUS_FRAME), 0, (struct sockaddr *) &remoteServAddr,
                sizeof (remoteServAddr));
        usleep((__useconds_t) 2e5); // 5Hz
    }
    close(fd);
    return 0;
}