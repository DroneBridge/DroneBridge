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
#include "parameter.h"
#include "rc_ground.h"
#include "../common/db_raw_send_receive.h"
#include "../common/ccolors.h"
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
 * Look for the i6S controller on the given interface. Reinitialize if it was unplugged.
 * @param new_Joy_IF Number of the joystick interface
 * @param calibrate_comm The command to be executed to calibrate the i6S
 * @return The file descriptor
 */
int initialize_i6S(int new_Joy_IF) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    printf("DB_CONTROL_GROUND: Waiting for i6S to be detected on: %s\n", interface_joystick);
    do {
        usleep(100000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keepRunning);
    printf("DB_CONTROL_GROUND: Opened joystick interface!\n");
    printf("DB_CONTROL_GROUND: Calibrating...\n");
    int returnval = system(DEFAULT_i6S_CALIBRATION); // i6S always calibrated by hard coded string - adjustrc does not have any effect
//    char calibration_command[500];
//    sprintf(calibration_command, "%s %s", "jscal-restore", interface_joystick);
//    int returnval = system(calibration_command);
    if (returnval == 0) {
        printf("DB_CONTROL_GROUND: Calibrated i6S\n");
    }else{
        printf("DB_CONTROL_GROUND: Could not calibrate i6S\n");
    }
    return fd;
}

/**
 * Transform the values read from the RC to values between 1000 and 2000
 * @param value The value read from the interface
 * @param adjustingValue A extra value that might add extra exponential behavior to the sticks etc.
 * @return 1000<=return_value<=2000
 */
uint16_t normalize_i6S(int16_t value, int16_t adjustingValue) {
    return (uint16_t) (((adjustingValue * value) / MAX) + 1500);
}


int i6S(int Joy_IF, char calibrate_comm[]) {
    signal(SIGINT, intHandler);
    uint16_t JoystickData[NUM_CHANNELS];
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 16666666L; //60Hz
    //tim.tv_nsec = 10000000L; //100Hz


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

    printf("DB_CONTROL_GROUND: Starting to send commands!\n");
    while (keepRunning) //send loop
    {
        nanosleep(&tim, &tim2);
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
                printf(RED "DB_CONTROL_GROUND: Joystick was unplugged! Retrying... " RESET "\n");
                fd = initialize_i6S(Joy_IF);
            } else {
                printf(RED "DB_CONTROL_GROUND: Error: %s" RESET " \n", strerror(myerror));
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
        JoystickData[0] = normalize_i6S(rc.roll, 500);
        JoystickData[1] = normalize_i6S(rc.pitch, 500);
        JoystickData[2] = normalize_i6S(rc.throttle, 500);
        JoystickData[3] = normalize_i6S(rc.yaw, 500);
        JoystickData[4] = normalize_i6S(rc.cam_up, 500);
        JoystickData[5] = normalize_i6S(rc.cam_down, 500);
        JoystickData[6] = (uint16_t) rc.button0;
        JoystickData[7] = (uint16_t) rc.pos_switch1;
        JoystickData[8] = (uint16_t) rc.pos_switch2;
        JoystickData[9] = (uint16_t) rc.button5;
        JoystickData[10] = 1000; // unused by i6s - used by app
        JoystickData[11] = 1000; // unused by i6s - used by app
        JoystickData[12] = 1000; // unused by i6s - used by app
        JoystickData[13] = 1000; // unused by i6s - used by app
        send_rc_packet(JoystickData);
    }
    close(fd);
    close_socket_send_receive();
    return 0;
}