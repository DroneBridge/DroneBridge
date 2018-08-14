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

#include <stdint-gcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "db_utils.h"


void print_buffer(uint8_t buffer[], int num_bytes){
    for (int i = 0; i < num_bytes; ++i) {
        printf("%2x ", buffer[i]);
    }
    printf("\n");
}

/**
 * Reads the current undervoltage value from RPi
 * @return 1 if currently not enough voltage supplied to Pi; 0 if all OK
 */
uint8_t get_undervolt(void){
    uint8_t uvolt = 10;
    FILE *fp;
    char path[1035];
    fp = popen("vcgencmd get_throttled", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }
    if (fgets(path, sizeof(path)-1, fp) != NULL) {
        char *dummy;
        uint32_t iuvolt = (uint32_t) strtoul(&path[10], &dummy, 16);
        uvolt = (uint8_t) ((uint8_t) iuvolt & 1);  // get undervolt bit - current voltage status
    }
    pclose(fp);
    return uvolt;
}
