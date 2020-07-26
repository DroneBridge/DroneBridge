/*
 *   This file is part of DroneBridge: https://github.com/DroneBridge/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
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
#include <stdint-gcc.h>
#include <time.h>
#include <bits/time.h>
#include "fec_speed_test.h"
#include "fec.h"

#define FILE_PATH   "/home/cyber/Bilder/20200515_192108.jpg"

char *buffered_file;

long buffer_file(char *filepath);

int main(int argc, char *argv[]) {
    unsigned int packet_size = 1024;
    unsigned int num_data_blocks = 8;
    unsigned int num_fec_blocks = 4;

    struct timespec start_time, end_time;
    uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
    uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];

    long filesize = buffer_file(FILE_PATH);
    fec_init();

    for (int i = 0; i < num_fec_blocks; ++i) {
        fec_blocks[i] = fec_pool[i];
    }

    int file_position = 0;

    int size = 1024;
    int multiplier = 3;
    unsigned char src[size];
    unsigned char dst[size];
    srand(time(NULL));
    for (int i = 0; i < size; i++)
        src[i] = (rand()%(90-65))+65;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < 100; i++)
        slow_mul1(dst, src, multiplier, size);
//    do {
//        for (int i = 0; i < num_data_blocks; ++i) {
//            data_blocks[i] = &buffered_file[file_position + i * packet_size];
//        }
//        fec_encode(packet_size, data_blocks, num_data_blocks, (unsigned char **) fec_blocks, num_fec_blocks);
////    fec_decode(packet_size, data_blocks, num_data_blocks, fec_blocks, fec_block_nos, erased_blocks, nr_fec_blocks);
//        file_position += num_data_blocks * packet_size;
//    } while (file_position < filesize);
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    free(buffered_file);
    printf("Done in %i microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    return 0;
}

long buffer_file(char *filepath) {
    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    buffered_file = malloc(fsize + 1);
    fread(buffered_file, 1, fsize, f);
    fclose(f);
    buffered_file[fsize] = 0;
    return fsize;
}