/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2018 Wolfgang Christl
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
#include <signal.h>
#include <time.h>
#include <zconf.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <memory.h>
#include <linux/joystick.h>
#include "opentx.h"
#include "../common/db_protocol.h"
#include "../common/db_common.h"
#include "parameter.h"
#include "rc_ground.h"

static volatile int keep_running = 1;

void custom_signal_handler(int dummy) {
    keep_running = 0;
}

/**
 * Look for the OpenTX controller on the given interface. Reinitialize if it was unplugged.
 *
 * @param joy_interface_indx Number of the joystick interface
 * @param calibrate_comm The command to be executed to calibrate the OpenTX controller
 * @return The file descriptor
 */
int initialize_opentx(int joy_interface_indx) {
    int fd;
    char path_interface_joystick[500];  // eg. /dev/input/js0 with 0 as the interface index
    get_joy_interface_path(path_interface_joystick, joy_interface_indx);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Waiting for OpenTX RC to be detected on: %s\n", path_interface_joystick);
    do {
        usleep(100000);
        fd = open(path_interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keep_running);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Opened joystick interface!\n");
    char calibrate_comm[CALI_COMM_SIZE];
    strcpy(calibrate_comm, DEFAULT_OPENTX_CALIBRATION);
    do_calibration(calibrate_comm, joy_interface_indx);
    return fd;
}

/**
 * Transform the values read from the RC to values between 1000 and 2000
 *
 * @param value The value read from the interface
 * @param adjustingValue A extra value that might add extra exponential behavior to the sticks etc.
 * @return 1000<=return_value<=2000
 */
uint16_t normalize_opentx(int16_t value) {
    return (uint16_t) (((500 * value) / MAX) + 1500);
}

/**
 * Read and send RC commands using a OpenTX based radio
 *
 * @param Joy_IF Joystick interface as specified by jscal interface index of the OpenTX based radio connected via USB
 * @param frequency_sleep Time to sleep between every RC value read & send
 */
void opentx(int Joy_IF, struct timespec frequency_sleep) {
    signal(SIGINT, custom_signal_handler);
    struct js_event e;
    uint16_t joystickData[NUM_CHANNELS];
    struct timespec tim_remain;
    int16_t opentx_channels[32] = {0};

    int fd = initialize_opentx(Joy_IF);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: DroneBridge OpenTX - starting!\n");
    while (keep_running) //send loop
    {
        nanosleep(&frequency_sleep, &tim_remain);
        while (read(fd, &e, sizeof(e)) > 0)   // go through all events occurred
        {
            e.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
            if (e.type == JS_EVENT_AXIS) {
                opentx_channels[e.number] = e.value;
            } else if (e.type == JS_EVENT_BUTTON) {
                opentx_channels[8 + e.number] = e.value;
            }
        }

        int myerror = errno;
        if (myerror != EAGAIN) {
            if (myerror == ENODEV) {
                LOG_SYS_STD(LOG_WARNING, "DB_CONTROL_GND: Joystick was unplugged! Retrying...\n");
                fd = initialize_opentx(Joy_IF);
            } else {
                LOG_SYS_STD(LOG_ERR, "DB_CONTROL_GND: Error: %s\n", strerror(myerror));
            }
        }
        // Channel map must be AETR1234!
        joystickData[0] = normalize_opentx(opentx_channels[0]);
        joystickData[1] = normalize_opentx(opentx_channels[1]);
        joystickData[2] = normalize_opentx(opentx_channels[2]);
        joystickData[3] = normalize_opentx(opentx_channels[3]);
        joystickData[4] = normalize_opentx(opentx_channels[4]);
        joystickData[5] = normalize_opentx(opentx_channels[5]);
        joystickData[6] = normalize_opentx(opentx_channels[6]);
        joystickData[7] = normalize_opentx(opentx_channels[7]);
        if (opentx_channels[8] == 1) joystickData[8] = (uint16_t) 1000; else joystickData[8] = (uint16_t) 2000;
        if (opentx_channels[9] == 1) joystickData[9] = (uint16_t) 1000; else joystickData[9] = (uint16_t) 2000;
        if (opentx_channels[10] == 1) joystickData[10] = (uint16_t) 1000; else joystickData[10] = (uint16_t) 2000;
        if (opentx_channels[11] == 1) joystickData[11] = (uint16_t) 1000; else joystickData[11] = (uint16_t) 2000;
        if (opentx_channels[12] == 1)
            joystickData[12] = (uint16_t) 1000;
        else joystickData[12] = (uint16_t) 2000; // not sent via DB RC proto
        if (opentx_channels[13] == 1)
            joystickData[13] = (uint16_t) 1000;
        else joystickData[13] = (uint16_t) 2000; // not sent via DB RC proto
        send_rc_packet(joystickData);
    }
    close(fd);
    close_raw_interfaces();
}