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
#include <stdlib.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include "parameter.h"
#include "../common/db_protocol.h"
#include "i6S.h"
#include "../common/db_raw_send_receive.h"
#include "rc_ground.h"
#include "../common/ccolors.h"
#include "opentx.h"

int detect_RC(int new_Joy_IF) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    printf(YEL "DB_CONTROL_GROUND: Waiting for a RC to be detected on: %s" RESET "\n", interface_joystick);
    do {
        usleep(250000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0);
    return fd;
}

int main(int argc, char *argv[]) {
    atexit(close_socket_send_receive);
    char ifName[IFNAMSIZ], RC_name[128];
    char calibrate_comm[500];
    uint8_t comm_id;
    int Joy_IF, c, bitrate_op, rc_protocol;
    char db_mode = 'm'; char allow_rc_overwrite = 'N';

    // Command Line processing
    Joy_IF = JOY_INTERFACE;
    rc_protocol = 5;
    bitrate_op = DEFAULT_BITRATE_OPTION;
    comm_id = DEFAULT_V2_COMMID;
    strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
    strcpy(ifName, DEFAULT_IF);
    opterr = 0;
    while ((c = getopt(argc, argv, "n:j:m:b:g:v:o:c:")) != -1) {
        switch (c) {
            case 'n':
                strncpy(ifName, optarg, IFNAMSIZ);
                break;
            case 'j':
                Joy_IF = (int) strtol(optarg, NULL, 10);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'g':
                strcpy(calibrate_comm, optarg);
                break;
            case 'v':
                rc_protocol = (int) strtol(optarg, NULL, 10);
                break;
            case 'o':
                allow_rc_overwrite = *optarg;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("12ch RC via the DB-RC option (-v 5)\n");
                printf("14ch RC using FC serial protocol (-v 1|2|4)\n");
                printf("Use following commandline arguments.\n");
                printf("-n network interface for long range \n"
                               "-j number of joystick interface of RC \n"
                               "-m mode: [w|m] for wifi or monitor\n"
                               "-v Protocol [1|2|4|5]: 1 = MSPv1 [Betaflight/Cleanflight]; 2 = MSPv2 [iNAV]; "
                               "3 = MAVLink v1 (unsupported); 4 = MAVLink v2; 5 = DB-RC (default)\n"
                               "-o [Y|N] enable/disable RC overwrite\n"
                               "-c [communication id] Choose a number from 0-255. Same on groundstation and drone!\n"
                               "-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n(bitrate option only supported with Ralink chipsets)\n");
                return -1;
            default:
                abort();
        }
    }

    if (open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_CONTROLLER) < 0) {
        printf(RED "DB_CONTROL_GROUND: Could not open socket " RESET "\n");
        exit(-1);
    }
    conf_rc(rc_protocol, allow_rc_overwrite);
    open_rc_shm();

    int sock_fd = detect_RC(Joy_IF);
    if (ioctl(sock_fd, JSIOCGNAME(sizeof(RC_name)), RC_name) < 0)
        strncpy(RC_name, "Unknown", sizeof(RC_name));
    close(sock_fd); // We reopen in the RC specific file. Only opened in here to get the name of the controller
    printf(GRN "DB_CONTROL_GROUND: Detected \"%s\"" RESET "\n", RC_name);
    if (strcmp(i6S_descriptor, RC_name) == 0){
        printf("DB_CONTROL_GROUND: Choosing i6S-Config\n");
        strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
        i6S(Joy_IF, calibrate_comm);
    } else {
        printf("DB_CONTROL_GROUND: Choosing OpenTX-Config\n");
        strcpy(calibrate_comm, DEFAULT_OPENTX_CALIBRATION);
        opentx(Joy_IF, calibrate_comm);
    }
    return 0;
}