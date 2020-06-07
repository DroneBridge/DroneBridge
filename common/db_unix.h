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

#ifndef DRONEBRIDGE_DB_UNIX_H
#define DRONEBRIDGE_DB_UNIX_H

#include <sys/un.h>

#define DB_MAX_UNIX_TCP_CLIENTS     5       // Max number of connected clients to a TCP unix domain socket

#define DB_UNIX_DOMAIN_VIDEO_PATH   "/tmp/db_video_out"
#define DB_UNIX_TCP_SERVER_CONTROL  "/tmp/db_control_out"

typedef struct {
    int socket;
    struct sockaddr_un addr;
} db_unix_tcp_socket;

typedef struct {
    int client_sock;
    struct sockaddr_un addr;
} db_unix_tcp_client;

db_unix_tcp_socket db_create_unix_tcpserver_sock(char *filepath);

#endif //DRONEBRIDGE_DB_UNIX_H
