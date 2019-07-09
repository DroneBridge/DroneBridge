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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "recorder.h"

volatile uint32_t write_count = 0;

void write_to_file(uint8_t buffer[], uint16_t data_length, FILE *file) {
    if (file != NULL)
        fwrite(buffer, data_length, 1, file);
}

/**
 * Count number of .h264 video files inside /boot/recordings
 * @return
 */
int count_files() {
    int len, count = 0;
    struct dirent *pDirent;
    DIR *pDir;

    pDir = opendir("/boot/recordings");
    if (pDir != NULL) {
        while ((pDirent = readdir(pDir)) != NULL) {
            len = strlen(pDirent->d_name);
            if (len >= 4)
                if (strcmp (".h264", &(pDirent->d_name[len - 5])) == 0)
                    count++;
        }
        closedir(pDir);
    }
}

FILE *open_new_file() {
    char filename[100] = "/boot/recordings";
    sprintf(filename + 16,"%i", count_files());
    FILE *file_pnter;
    file_pnter = fopen(filename, "ab+");
    return file_pnter;
}

/**
 * Consumer thread main function
 */
void recorder(void) {
    uint32_t num_new_bytes;
    FILE *file_pnter; file_pnter = NULL;

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