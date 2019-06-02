/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2019 Wolfgang Christl
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

#ifndef DRONEBRIDGE_TCP_SERVER_H
#define DRONEBRIDGE_TCP_SERVER_H
struct tcp_server_info {
    int sock_fd;
    struct sockaddr_in servaddr;
};

struct tcp_server_info create_tcp_server_socket(uint port);
void send_to_all_tcp_clients(const int list_client_sockets[], const uint8_t message[], int message_length);
#endif //DRONEBRIDGE_TCP_SERVER_H
