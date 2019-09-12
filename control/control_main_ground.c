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
#include "rc_ground.h"
#include "opentx.h"
#include "../common/db_common.h"

#define DB_DEFAULT_RC_FREQUENCY 60

int detect_RC(int new_Joy_IF) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Waiting for a RC to be detected on: %s\n", interface_joystick);
    do {
        usleep(250000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0);
    return fd;
}

int main(int argc, char *argv[]) {
    atexit(close_raw_interfaces);
    char RC_name[128];
    char calibrate_comm[CALI_COMM_SIZE] = {'\0'};
    uint8_t comm_id, frame_type;
    int rc_int_indx, c, bitrate_op, rc_protocol, adhere_80211;
    char db_mode = 'm';
    char allow_rc_overwrite = 'N';
    int num_inf_rc = 0, rc_frequency = DB_DEFAULT_RC_FREQUENCY;
    char adapters[DB_MAX_ADAPTERS][IFNAMSIZ];

    // Command Line processing
    rc_int_indx = JOY_INTERFACE;
    rc_protocol = 5;
    bitrate_op = 1;
    adhere_80211 = 0;
    comm_id = DEFAULT_V2_COMMID;
    frame_type = DB_FRAMETYPE_DEFAULT;
    opterr = 0;
    while ((c = getopt(argc, argv, "n:j:m:b:g:v:o:t:c:a:")) != -1) {
        switch (c) {
            case 'n':
                if (num_inf_rc < DB_MAX_ADAPTERS) {
                    strncpy(adapters[num_inf_rc], optarg, IFNAMSIZ);
                    num_inf_rc++;
                }
                break;
            case 'j':
                rc_int_indx = (int) strtol(optarg, NULL, 10);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
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
            case 't':
                frame_type = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'a':
                adhere_80211 = (int) strtol(optarg, NULL, 10);
                break;
            case 'r':
                rc_frequency = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("12ch RC via the DB-RC option (-v 5)\n");
                printf("14ch RC using FC serial protocol (-v 1|2|4)\n");
                printf("Use following commandline arguments.");
                printf("\n\t-n Network interface(s) for long range"
                       "\n\t-j Number of joystick interface of RC"
                       "\n\t-m Mode: [w|m] for wifi or monitor"
                       "\n\t-v Protocol [1|2|4|5]: 1 = MSPv1 [Betaflight/Cleanflight]; 2 = MSPv2 [iNAV]; "
                       "3 = MAVLink v1 (unsupported); 4 = MAVLink v2; 5 = DB-RC (default)"
                       "\n\t-o [Y|N] enable/disable RC overwrite"
                       "\n\t-c [communication id] Choose a number from 0-255. Same on ground station and UAV!"
                       "\n\t-t <1|2> DroneBridge v2 raw protocol packet/frame type: 1=RTS, 2=DATA (CTS protection)"
                       "\n\t-r RC frequency in Hz (default %i Hz)"
                       "\n\t-b Bit rate in Mbps: (1|2|5|6|9|11|12|18|24|36|48|54)\n\t\t(bitrate option only "
                       "supported with Ralink chipsets), default is %i Mbps."
                       "\n\t-a <0|1> to enable/disable. Offsets the payload by some bytes so that it sits outside "
                       "then 802.11 header.\n\t\t Set this to 1 if you are using a non DB-Rasp Kernel!\n",
                       DB_DEFAULT_RC_FREQUENCY, bitrate_op);
                exit(0);
            default:
                abort();
        }
    }
    conf_rc(adapters, num_inf_rc, comm_id, db_mode, bitrate_op, frame_type, rc_protocol, allow_rc_overwrite,
            adhere_80211);

    open_rc_shm();

    // calculate sleep time based on frequency
    if (rc_frequency <= 0) {
        LOG_SYS_STD(LOG_ERR, "DB_CONTROL_GND: RC frequency (Hz) must be >0\n");
        rc_frequency = DB_DEFAULT_RC_FREQUENCY;
    }
    double sleep_time_nano_sec = 1e9 / rc_frequency;
    struct timespec sleep_time;  // time to sleep between every send of RC command
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = sleep_time_nano_sec;

    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: started!\n");
    int sock_fd = detect_RC(rc_int_indx);
    if (ioctl(sock_fd, JSIOCGNAME(sizeof(RC_name)), RC_name) < 0)
        strncpy(RC_name, "Unknown", sizeof(RC_name));

    close(sock_fd); // We reopen in the RC specific file. Only opened in here to get the name of the controller
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Detected \"%s\"\n", RC_name);
    if (strcmp(i6S_descriptor, RC_name) == 0) {
        LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Choosing i6S-Config\n");
        i6S(rc_int_indx, sleep_time);
    } else {
        LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Choosing OpenTX-Config\n");
        strcpy(calibrate_comm, DEFAULT_OPENTX_CALIBRATION);
        do_calibration(calibrate_comm, rc_int_indx);
        opentx(rc_int_indx, sleep_time);
    }
    exit(0);
}