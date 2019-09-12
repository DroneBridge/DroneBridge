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
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../common/db_common.h"
#include "parameter.h"
#include "../common/db_protocol.h"
#include "rc_ground.h"
#include "i6S.h"

#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */

#define MAX 32767

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

/**
 * Open socket to connected radio. Calibrate using jscal-restore command
 *
 * @param joy_interface_indx Joystick interface as specified by jscal interface index
 * @return The file descriptor
 */
int initialize_i6S(int joy_interface_indx) {
    char interface_joystick[CALI_COMM_SIZE];
    get_joy_interface_path(interface_joystick, joy_interface_indx);
    int fd;
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Waiting for i6S to be detected on: %s\n", interface_joystick);
    do {
        usleep(100000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keepRunning);
    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Opened joystick interface!\n");

    char calibrate_comm[CALI_COMM_SIZE];
    strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
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
uint16_t normalize_i6S(int16_t value, int16_t adjustingValue) {
    return (uint16_t) (((adjustingValue * value) / MAX) + 1500);
}

/**
 * Read and send RC commands using a i6S radio controller connected via USB.
 *
 * @param Joy_IF Joystick interface as specified by jscal of the OpenTX based radio connected via USB
 * @param frequency_sleep Time to sleep between every RC value read & send
 */
void i6S(int Joy_IF, struct timespec frequency_sleep) {
    signal(SIGINT, intHandler);
    uint16_t joystickData[NUM_CHANNELS];
    struct timespec tim_remain;

    struct js_event {
        unsigned int time;      /* event timestamp in milliseconds */
        short value;            /* value */
        unsigned char type;     /* event type */
        unsigned char number;   /* axis/button number */
    };

    struct i6SRC {
        int16_t roll;
        int16_t pitch;
        int16_t throttle;
        int16_t yaw;
        int16_t cam_up;
        int16_t cam_down;
        int16_t button0;
        int16_t button1;
        int16_t button2;
        int16_t button3;
        int16_t button4;
        int16_t button5;
        int16_t pos_switch1;
        int16_t pos_switch2;
    };

    int fd = initialize_i6S(Joy_IF);

    struct js_event e;
    struct i6SRC rc;
    rc.roll = 0;
    rc.pitch = 0;
    rc.throttle = 0;
    rc.yaw = 0;
    rc.cam_up = 0;
    rc.cam_down = 0;
    rc.button0 = 1;
    rc.button1 = 0;
    rc.button2 = 1;
    rc.button3 = 0;
    rc.button4 = 1;
    rc.button5 = 0;
    rc.pos_switch1 = 1000;
    rc.pos_switch2 = 1000;

    LOG_SYS_STD(LOG_INFO, "DB_CONTROL_GND: Starting to send commands!\n");
    while (keepRunning) //send loop
    {
        nanosleep(&frequency_sleep, &tim_remain);
        while (read(fd, &e, sizeof(e)) > 0)   // go through all events occurred
        {
            e.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
            if (e.type == JS_EVENT_AXIS) {
                switch (e.number) {
                    case 0:
                        rc.roll = e.value;
                        break;
                    case 1:
                        rc.pitch = e.value;
                        break;
                    case 2:
                        rc.throttle = e.value;
                        break;
                    case 3:
                        rc.yaw = e.value;
                        break;
                    case 4:
                        rc.cam_up = e.value;
                        break;
                    case 5:
                        rc.cam_down = e.value;
                        break;
                    default:
                        break;
                }
            } else if (e.type == JS_EVENT_BUTTON) {
                switch (e.number) {
                    case 0:
                        rc.button0 = e.value;
                        break;
                    case 1:
                        rc.button1 = e.value;
                        break;
                    case 2:
                        rc.button2 = e.value;
                        break;
                    case 3:
                        rc.button3 = e.value;
                        break;
                    case 4:
                        rc.button4 = e.value;
                        break;
                    case 5:
                        rc.button5 = e.value;
                        break;
                    default:
                        break;
                }
            }
        }
        int myerror = errno;
        if (myerror != EAGAIN) {
            if (myerror == ENODEV) {
                LOG_SYS_STD(LOG_WARNING, "DB_CONTROL_GND: Joystick was unplugged! Retrying... \n");
                fd = initialize_i6S(Joy_IF);
            } else {
                LOG_SYS_STD(LOG_ERR, "DB_CONTROL_GND: Error: %s\n", strerror(myerror));
            }
        }
        // SWR - Arm switch
        if (rc.button0 == 1) { rc.button0 = 1000; } else if (rc.button0 == 0) { rc.button0 = 2000; }
        // SWD - failsafe
        if (rc.button5 == 0) { rc.button5 = 1000; } else if (rc.button5 == 1) { rc.button5 = 2000; }

        // SWB - 3pos switch 1
        if (rc.button1 == 0 && rc.button2 == 1) {
            rc.pos_switch1 = 1000;
        } else if (rc.button1 == 0 && rc.button2 == 0) {
            rc.pos_switch1 = 1500;
        } else {
            rc.pos_switch1 = 2000;
        }

        // SWC - 3pos switch 2
        if (rc.button3 == 0 && rc.button4 == 1) {
            rc.pos_switch2 = 1000;
        } else if (rc.button3 == 0 && rc.button4 == 0) {
            rc.pos_switch2 = 1500;
        } else {
            rc.pos_switch2 = 2000;
        }
        //adjust endpositions and buttons (with proper calibration not necessary)
        if (rc.roll == 32766) rc.roll++;
        if (rc.pitch == 32766) rc.pitch++;
        if (rc.throttle == 32766) rc.throttle++;
        if (rc.yaw == 32766) rc.yaw++;

        // Channel map should/must be AETR1234!
        joystickData[0] = normalize_i6S(rc.roll, 500);
        joystickData[1] = normalize_i6S(rc.pitch, 500);
        joystickData[2] = normalize_i6S(rc.throttle, 500);
        joystickData[3] = normalize_i6S(rc.yaw, 500);
        joystickData[4] = normalize_i6S(rc.cam_up, 500);
        joystickData[5] = normalize_i6S(rc.cam_down, 500);
        joystickData[6] = (uint16_t) rc.button0;
        joystickData[7] = (uint16_t) rc.pos_switch1;
        joystickData[8] = (uint16_t) rc.pos_switch2;
        joystickData[9] = (uint16_t) rc.button5;
        joystickData[10] = 1000; // unused by i6s - used by app
        joystickData[11] = 1000; // unused by i6s - used by app
        joystickData[12] = 1000; // unused by i6s - used by app
        joystickData[13] = 1000; // unused by i6s - used by app
        send_rc_packet(joystickData);
    }
    close(fd);
    close_raw_interfaces();
}