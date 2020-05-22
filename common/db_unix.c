/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "db_common.h"
#include "db_unix.h"

db_unix_tcp_socket db_create_unix_tcpserver_sock(char *filepath) {
    db_unix_tcp_socket db_unix_sock = {.socket = -1, .addr = 0};
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));
    if ((db_unix_sock.socket = socket(AF_LOCAL, SOCK_STREAM, 0)) > 0) {
        LOG_SYS_STD(LOG_ERR, "Could not create unix tcp server socket\n");
        return db_unix_sock;
    }
    unlink(filepath);
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, filepath);
    if (bind(db_unix_sock.socket, (struct sockaddr *) &db_unix_sock.addr, sizeof(db_unix_sock.addr)) != 0) {
        LOG_SYS_STD(LOG_ERR, "Could not bind unix tcp server socket to %s\n", filepath);
        return db_unix_sock;
    }
    return db_unix_sock;
}