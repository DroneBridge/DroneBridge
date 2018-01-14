//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#ifndef STATUS_DB_GET_IP_H
#define STATUS_DB_GET_IP_H

#define IP_LENGTH 15

int init_shared_memory_ip();
char * get_ip_from_ipchecker(int shmid);

#endif //STATUS_DB_GET_IP_H
