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

#ifndef DRONEBRIDGE_DB_USB_H
#define DRONEBRIDGE_DB_USB_H

#define DB_USB_PROTO_VERSION    1

#define DB_USB_PARSER_SEARCHING_HEADER  0   // wait for header to arrive at start of USB packet buffer
#define DB_USB_PARSER_AWAITING_PAYLOAD  1   // wait for payload completion

#define MAX_POLL_FDS                    20
#define DB_USB_PORT_TIMEOUT_WAKE        255

struct poll_fd_count_t {
    int total_poll_fd_cnt;
    int usb_poll_fd_cnt;
} poll_fd_count;

#endif //DRONEBRIDGE_DB_USB_H
