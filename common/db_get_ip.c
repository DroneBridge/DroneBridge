/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2017 Wolfgang Christl
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
        return "127.0.0.1";
    }
    return ip_checker_ip;
}