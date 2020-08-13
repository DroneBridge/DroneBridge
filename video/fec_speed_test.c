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
#include <time.h>
#include <bits/time.h>
#include <string.h>
//#include <gf_complete.h>

#include "fec_speed_test.h"
#include "gf256.h"
#include "fec_faster.h"
//#include <moepgf/moepgf.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

char *buffered_file;

long buffer_file(char *filepath);

int main(int argc, char *argv[]) {
    unsigned int packet_size = 1024;
    unsigned int num_data_blocks = 8;
    unsigned int num_fec_blocks = 4;
//    gf_t gf;

    struct timespec start_time, end_time;
    uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
    uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    fec_init();
//    if (!gf_init_easy(&gf, 8)) {
//        fprintf(stderr, "Couldn't initialize GF structure.\n");
//        exit(0);
//    }
    int c;
    if ((c = gf256_init()) < 0) {
        fprintf(stderr, "Couldn't initialize GF256 (Error Code %i)\n", c);
        exit(0);
    }
//    struct moepgf moep_gf;
//    if (moepgf_init(&moep_gf, MOEPGF256, MOEPGF_ALGORITHM_BEST) < 0) {
//        fprintf(stderr, "Couldn't initialize moepgf\n");
//        exit(0);
//    }


    for (int i = 0; i < num_fec_blocks; ++i) {
        fec_blocks[i] = fec_pool[i];
    }

    int file_position = 0;

    int size = packet_size;
    uint8_t multiplier = 56;
    uint8_t src[size];
    uint8_t dsto[size];
    uint8_t dst_mul[size];
    uint8_t dst_addmul[size];

    int equal = 1;
    srand(time(NULL));
    for (int i = 0; i < size; i++)
        src[i] = (rand() % (90 - 65)) + 65;

    int iter = 500;
    printf("Doing %i iterations on %i FEC blocks\n", iter, num_fec_blocks);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int j = 0; j < iter; j++) {
        for (int i = 0; i < num_fec_blocks; i++)
            slow_mul1(dsto, src, multiplier, size);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("\tDone original in %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int j = 0; j < 1000; j++) {
        for (int i = 0; i < num_fec_blocks; i++)
//            moep_gf.mulrc(dst_mul, multiplier, size);
            gf256_mul_mem(dst_mul, src, multiplier, size);
//            gf.multiply_region.w32(&gf, src, dst_mul, multiplier, size, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("\tDone new in %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    if (memcmp(dsto, dst_mul, size) == 0) {
    } else {
        equal = 0;
    }
    if (equal) {
        printf(KGRN"Both mul functions produce equal results!\n"KNRM);
    } else {
        printf(KRED "Multiplication results not byte-equal!\n"KNRM);
    }

    printf("\n");
    srand(time(NULL));
    for (int i = 0; i < size; i++)
        src[i] = (rand() % (90 - 65)) + 65;
    memset(dsto, 15, size);
    memset(dst_addmul, 15, size);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < (num_fec_blocks * num_data_blocks); i++)
        slow_addmul1(dsto, src, multiplier, size);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("Done original in %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < (num_fec_blocks * num_data_blocks); i++) {
        gf256_muladd_mem(dst_addmul, multiplier, src, size);
//        moep_gf.maddrc(dst_addmul, multiplier, src, size);
//        gf.multiply_region.w32(&gf, src, dst_addmul, multiplier, size, 1);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("Done new in %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    if (memcmp(dsto, dst_addmul, size) == 0) {
        printf(KGRN"Both addmul functions produce equal results!\n"KNRM);
    } else {
        printf(KRED "Add-Multiplication results not byte-equal!\n"KNRM);
    }

    uint8_t matrix[] = {1, 5, 10, 11, 10, 0, 15, 0, 0, 5, 7, 9, 3, 4, 2, 12};
    double omatrix[] = {1, 5, 10, 11, 10, 0, 15, 0, 0, 5, 7, 9, 3, 4, 2, 12};
    double destomatrix[num_fec_blocks * num_fec_blocks];
    printf("\nInverting matrix\n");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    int ro = invert_mat(matrix, num_fec_blocks);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("Done old in %.02f microseconds %i\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time), ro);

    uint8_t z = gf256_add( 128,  128);

//    do {
//        for (int i = 0; i < num_data_blocks; ++i) {
//            data_blocks[i] = &buffered_file[file_position + i * packet_size];
//        }
//        fec_encode(packet_size, data_blocks, num_data_blocks, (unsigned char **) fec_blocks, num_fec_blocks);
////    fec_decode(packet_size, data_blocks, num_data_blocks, fec_blocks, fec_block_nos, erased_blocks, nr_fec_blocks);
//        file_position += num_data_blocks * packet_size;
//    } while (file_position < filesize);

//    free(buffered_file);
    return 0;
}