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

// https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "recorder.h"

volatile uint32_t write_count = 0;

void write_to_file(uint8_t buffer[], uint16_t data_length, FILE *file) {
    if (file != NULL)
        fwrite(buffer, data_length, 1, file);
}

bool file_exists(char fname[]) {
    if (access(fname, F_OK) != -1)
        return true;
    else
        return false;
}

FILE *open_new_file() {
    char filename[100];
    struct tm *timenow;

    time_t now = time(NULL);
    timenow = localtime(&now);
    strftime(filename, sizeof(filename), "/DroneBridge/recordings/DB_VIDEO_%F_%H%M", timenow);
    if (file_exists(filename)) {
        for (int i = 0; i < 10; i++) {
            char str[12];
            sprintf(str, "_%d", i);
            strcat(filename, str);
            if (!file_exists(filename))
                break;
        }
    }

    FILE *file_pnter;
    file_pnter = fopen(filename, "ab+");
    return file_pnter;
}

/**
 * Consumer thread main function
 */
void recorder(void) {
    uint32_t num_new_bytes;
    FILE *file_pnter;
    file_pnter = NULL;
    // TODO: does not work like that. Use Semaphores?!
    while (recorder_running) {
        while ((num_new_bytes = receive_count - write_count) == 0) {
            usleep(1000); /* rec_buff is empty - wait for new data*/
        }
        if (num_new_bytes > 0)
            write_to_file(rec_buff, num_new_bytes, file_pnter);
        else if (num_new_bytes < 0) {
            // producer reseted the receive count --> old file is done --> open new one
            num_new_bytes = receive_count;
            file_pnter = open_new_file();
            printf("Opening new video file for writing\n");
        }
        ++write_count;
    }
}