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
#include <zconf.h>
#include <signal.h>
#include <unistd.h>
#include <wiringPi.h>

#ifdef USE_PI_INSTALL_PATH
    #include "/root/dronebridge/common/shared_memory.h"
#else
    #include "../../common/shared_memory.h"
#endif

#define GPIO_RC_CH_10   21
#define GPIO_RC_CH_11   22
#define GPIO_RC_CH_12   27

int keep_running = 1;

void intHandler(int dummy)
{
    keep_running = 0;
}

/**
 * Feel free to change and improve. E.g. with a config.ini file or a better logic. This one is not so great.
 * This is a sample/proof of concept application
 *
 * Sets GPIO pins to high if DB-RC value of specified channel is higher or equal than 1500.
 * Sets GPIO pins to low if DB-RC value of specified channel is lower than 1500.
 *
 * Uses DroneBridge RC channels 9 to 12.
 * Maps WiringPi pins:
 * 21->ch10
 * 22->ch11
 * 27->ch12
 *
 * For WiringPi pin definitions see: https://de.pinout.xyz/pinout/wiringpi
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    if (wiringPiSetup() == -1){
        printf("Could not init wiringPiSetup(). Closing...\n");
        return -1;
    }
    // open the shared memory where DB control module stores the channel values
    db_rc_values_t *rc_values = db_rc_values_memory_open();

    // create a local storage array to keep a map of what the GPIOs currently are set (1=high, 0=low)
    int gpio_states[3] = {0, 0, 0};

    pinMode(GPIO_RC_CH_10, OUTPUT);
    pinMode(GPIO_RC_CH_11, OUTPUT);
    pinMode(GPIO_RC_CH_12, OUTPUT);
    while (keep_running){
        // check CH10 and set if it is not already set to HIGH
        if (rc_values->ch[9] >= 1500 && !gpio_states[0]){
            gpio_states[0] = 1;
            digitalWrite(GPIO_RC_CH_10, HIGH);
        } else if(rc_values->ch[9] < 1500 && gpio_states[0]) {
            gpio_states[0] = 0;
            digitalWrite(GPIO_RC_CH_10, LOW);
        }

        // check CH11
        if (rc_values->ch[10] >= 1500 && !gpio_states[1]){
            gpio_states[1] = 1;
            digitalWrite(GPIO_RC_CH_11, HIGH);
        } else if(rc_values->ch[10] < 1500 && gpio_states[1]) {
            gpio_states[1] = 0;
            digitalWrite(GPIO_RC_CH_11, LOW);
        }

        // check CH12
        if (rc_values->ch[11] >= 1500 && !gpio_states[2]){
            gpio_states[2] = 1;
            digitalWrite(GPIO_RC_CH_12, HIGH);
        } else if(rc_values->ch[11] < 1500 && gpio_states[2]) {
            gpio_states[2] = 0;
            digitalWrite(GPIO_RC_CH_12, LOW);
        }

        usleep(500000); // sleep for 0.5 seconds
    }
}