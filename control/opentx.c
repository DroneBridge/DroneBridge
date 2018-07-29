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
#include <stdlib.h>
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
#include "../common/ccolors.h"
#include "rc_ground.h"
#include "../common/db_raw_send_receive.h"

static volatile int keepRunning = 1;

void opentx_Handler(int dummy) {
    keepRunning = 0;
}

/**
 * Look for the OpenTX controller on the given interface. Reinitialize if it was unplugged.
 * @param new_Joy_IF Number of the joystick interface
 * @param calibrate_comm The command to be executed to calibrate the OpenTX controller
 * @return The file descriptor
 */
int initialize_opentx(int new_Joy_IF) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    printf("DB_CONTROL_GROUND: Waiting for OpenTX RC to be detected on: %s\n", interface_joystick);
    do {
        usleep(100000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keepRunning);
    printf("DB_CONTROL_GROUND: Opened joystick interface!\n");
    printf("DB_CONTROL_GROUND: Calibrating...\n");
    char calibration_command[500];
    sprintf(calibration_command, "%s %s", "jscal-restore", interface_joystick);
    int returnval = system(calibration_command);
    if (returnval == 0) {
        printf("DB_CONTROL_GROUND: Calibrated OpenTX RC\n");
    }else{
        printf(RED "DB_CONTROL_GROUND: Could not calibrate OpenTX RC " RESET "\n");
    }
    return fd;
}

/**
 * Transform the values read from the RC to values between 1000 and 2000
 * @param value The value read from the interface
 * @param adjustingValue A extra value that might add extra exponential behavior to the sticks etc.
 * @return 1000<=return_value<=2000
 */
uint16_t normalize_opentx(int16_t value) {
    return (uint16_t) (((500 * value) / MAX) + 1500);
}

int opentx(int Joy_IF, char calibrate_comm[]) {
    signal(SIGINT, opentx_Handler);
    struct js_event e;
    uint16_t JoystickData[NUM_CHANNELS];
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 16666666L; //60Hz
    //tim.tv_nsec = 10000000L; //100Hz
    int16_t opentx_channels[32] = {0};

    int fd = initialize_opentx(Joy_IF);
    printf("DB_CONTROL_GROUND: DroneBridge OpenTX - starting!\n");
    while (keepRunning) //send loop
    {
        nanosleep(&tim, &tim2);
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
                printf(RED "DB_CONTROL_GROUND: Joystick was unplugged! Retrying... " RESET "\n");
                fd = initialize_opentx(Joy_IF);
            } else {
                printf(RED "DB_CONTROL_GROUND: Error: %s" RESET " \n", strerror(myerror));
            }
        }
        // Channel map must be AETR1234!
        JoystickData[0] = normalize_opentx(opentx_channels[0]);
        JoystickData[1] = normalize_opentx(opentx_channels[1]);
        JoystickData[2] = normalize_opentx(opentx_channels[2]);
        JoystickData[3] = normalize_opentx(opentx_channels[3]);
        JoystickData[4] = normalize_opentx(opentx_channels[4]);
        JoystickData[5] = normalize_opentx(opentx_channels[5]);
        JoystickData[6] = normalize_opentx(opentx_channels[6]);
        JoystickData[7] = normalize_opentx(opentx_channels[7]);
        if (opentx_channels[8] == 1) JoystickData[8] = (uint16_t) 1000; else JoystickData[8] = (uint16_t) 2000;
        if (opentx_channels[9] == 1) JoystickData[9] = (uint16_t) 1000; else JoystickData[9] = (uint16_t) 2000;
        if (opentx_channels[10] == 1) JoystickData[10] = (uint16_t) 1000; else JoystickData[10] = (uint16_t) 2000;
        if (opentx_channels[11] == 1) JoystickData[11] = (uint16_t) 1000; else JoystickData[11] = (uint16_t) 2000;
        if (opentx_channels[12] == 1) JoystickData[12] = (uint16_t) 1000; else JoystickData[12] = (uint16_t) 2000; // not sent via DB RC proto
        if (opentx_channels[13] == 1) JoystickData[13] = (uint16_t) 1000; else JoystickData[13] = (uint16_t) 2000; // not sent via DB RC proto
        send_rc_packet(JoystickData);
    }
    close(fd);
    close_socket_send_receive();
    return 0;
}