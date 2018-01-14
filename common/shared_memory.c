//
// Created by Wolfgang Christl on 08.12.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "db_protocol.h"
#include "lib.h"

wifibroadcast_rx_status_t *wbc_status_memory_open() {
    int fd;
    for(;;) {
        fd = shm_open("/wifibroadcast_rx_status_0", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd > 0) {
            break;
        }
        printf("db_rc_values_memory_open: Waiting for groundstation video to be started ... %s\n", strerror(errno));
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(wifibroadcast_rx_status_t)) == -1) {
        perror("db_rc_values_memory_open: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(wifibroadcast_rx_status_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("db_rc_values_memory_open: mmap");
        exit(1);
    }
    return (wifibroadcast_rx_status_t*)retval;
}

wifibroadcast_rx_status_t_rc *wbc_rc_status_memory_open() {
    int fd;
    for(;;) {
        fd = shm_open("/wifibroadcast_rx_status_rc", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd > 0) {
            break;
        }
        printf("wbc_rc_status_memory_open: Waiting for groundstation video to be started ... %s\n", strerror(errno));
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(wifibroadcast_rx_status_t_rc)) == -1) {
        perror("wbc_rc_status_memory_open: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(wifibroadcast_rx_status_t_rc), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("wbc_rc_status_memory_open: mmap");
        exit(1);
    }
    return (wifibroadcast_rx_status_t_rc*)retval;
}

void db_rc_values_memory_init(db_rc_values *rc_values) {
    for(int i = 0; i < NUM_CHANNELS; i++) {
        rc_values->ch[i] = 1000;
    }
}

db_rc_values *db_rc_values_memory_open() {
    int fd;
    for(;;) {
        fd = shm_open("/db_rc_values", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd > 0) {
            break;
        }
        printf("db_rc_values_memory_open: Waiting for init ... %s\n", strerror(errno));
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(db_rc_values)) == -1) {
        perror("db_rc_values_memory_open: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(db_rc_values), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("db_rc_values_memory_open: mmap");
        exit(1);
    }
    db_rc_values *tretval = (db_rc_values*)retval;
    db_rc_values_memory_init(tretval);
    return (db_rc_values*)retval;
}