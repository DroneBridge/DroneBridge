// based on rx_status by Rodizio. Based on wifibroadcast rx by Befinitiv. Licensed under GPL2
// integrated into the DroneBridge extensions by seeul8er
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
#include <sys/shm.h>
#include "lib.h"

#define SERVER_PORT 1604
#define IP_SHM_KEY 1111

bool volatile keeprunning = true;

#pragma pack(1)
struct DB_LTM_FRAME {
    uint8_t ident[3];
    uint8_t mode;
    int8_t cpu_usage;
    int8_t rssi_drone;
    int8_t rssi_ground;
    uint32_t damaged_blocks_wbc;
    uint32_t lost_packets_wbc;
    uint32_t kbitrate_wbc;
    uint8_t crc;
};
#pragma pack(0)
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
        printf("DB_STATUS_TX: Waiting for rx to be started ...\n");
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(wifibroadcast_rx_status_t)) == -1) {
        perror("DB_STATUS_TX: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(wifibroadcast_rx_status_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("DB_STATUS_TX: mmap");
        exit(1);
    }
    return (wifibroadcast_rx_status_t*)retval;
}


int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    char *myPtr;
    int restarts = 0, fd, shID, i, best_dbm = 0, cardcounter = 0;
    uint8_t counter = 0;
    struct DB_LTM_FRAME db_ltm_frame;
    db_ltm_frame.ident[0] = '$';
    db_ltm_frame.ident[1] = 'T';
    db_ltm_frame.ident[2] = 'Z';
    db_ltm_frame.mode = 'm';
    db_ltm_frame.cpu_usage = 0;
    db_ltm_frame.rssi_drone = 0;
    db_ltm_frame.crc = 0x66;

    wifibroadcast_rx_status_t *t = status_memory_open();
    int number_cards = t->wifi_adapter_cnt;

    struct sockaddr_in remoteServAddr;
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_addr.s_addr = inet_addr("192.168.2.2");
    remoteServAddr.sin_port = htons(SERVER_PORT);
    fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf ("DB_STATUS_TX: %s: Unable to open socket \n", strerror(errno));
        exit (EXIT_FAILURE);
    }
    int broadcast=1;
    if (setsockopt(fd,SOL_SOCKET,SO_BROADCAST, &broadcast,sizeof(broadcast))==-1) {
        printf("DB_STATUS_TX: %s",strerror(errno));
    }
    char ip_checker_ip[15];
    shID = shmget(IP_SHM_KEY, 15, 0666);

    printf("DB_STATUS_TX: started!");
    while(keeprunning) {
        counter++;
        // Get IP from IP Checker shared memory segment with key 1111 every 10th time
        if (shID >= 0) {
            if (counter>9){
                counter = 0;
                myPtr = shmat(shID, 0, 0);
                if (myPtr==(char *)-1) {
                    perror("shmat");
                } else {
                    memcpy(ip_checker_ip,myPtr,15);
                    shmdt(myPtr);
                }
            }
        } else {
            perror("shmget");
        }
        remoteServAddr.sin_addr.s_addr = inet_addr(ip_checker_ip);
        //printf("%s\n",ip_checker_ip);
        best_dbm = -1000;
        for(cardcounter=0; cardcounter<number_cards; ++cardcounter) {
            if (best_dbm < t->adapter[cardcounter].current_signal_dbm) best_dbm = t->adapter[cardcounter].current_signal_dbm;
        }
        db_ltm_frame.rssi_ground = best_dbm;
        db_ltm_frame.damaged_blocks_wbc = t->damaged_block_cnt;
        db_ltm_frame.lost_packets_wbc = t->lost_packet_cnt;
        db_ltm_frame.kbitrate_wbc = t-> kbitrate;
        if (t->tx_restart_cnt > restarts) {
            restarts++;
            usleep ((__useconds_t) 1e7);
        }

        sendto (fd, &db_ltm_frame, sizeof(struct DB_LTM_FRAME), 0, (struct sockaddr *) &remoteServAddr,
                sizeof (remoteServAddr));
        usleep((__useconds_t) 2e5); // 5Hz
    }
    close(fd);
    return 0;
}