//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <stdio.h>
#include <sys/shm.h>
#include <memory.h>
#include "db_get_ip.h"

#define IP_SHM_KEY 1111

/**
 *
 * @return shared memory id where IP form DroneBridge IP-Checker module is stored
 */
int init_shared_memory_ip(){
    return shmget(IP_SHM_KEY, 15, 0666);
}

/**
 * Returns the current IP form DroneBridge IP-Checker module
 * @param shmid The shared memory id returned by init_shared_memory_ip()
 * @return a char[] pointer to IP form IP checkers shared memory
 */
char * get_ip_from_ipchecker(int shmid){
    static char ip_checker_ip[IP_LENGTH];
    char *myPtr;

    if (shmid >= 0) {
        myPtr = shmat(shmid, 0, 0);
        if (myPtr==(char *)-1) {
            perror("shmat");
        } else {
            memcpy(ip_checker_ip, myPtr, IP_LENGTH);
            shmdt(myPtr);
        }
    } else {
        perror("shmget");
    }
    return ip_checker_ip;
}