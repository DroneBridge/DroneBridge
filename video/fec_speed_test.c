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

#include "fec_speed_test.h"
#include "gf256.h"
#include "fec_old.h"
#include "fec.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

int main(int argc, char *argv[]) {
    int packet_size = 1024;
    int num_data_blocks = 8;
    unsigned int num_fec_blocks = 4;
    struct timespec start_time, end_time;
    fec_init_old();
    fec_init();

    int size = packet_size;
    uint8_t multiplier = 56;
    uint8_t src[size];
    uint8_t dsto[size];
    uint8_t dst_mul[size];
    uint8_t dst_addmul[size];

    int equal = 1;
    srand(time(NULL));
    for (int i = 0; i < size; i++)
        src[i] = (rand() % (254 - 0 + 1)) + 0;

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
        src[i] = (rand() % (254 - 0 + 1)) + 0;
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

    const int test_data_buff_size = packet_size * 12;   //10s video with 6Mbps
    uint8_t test_data[test_data_buff_size];
    uint8_t *data_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t *data_blocks_old[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t fec_pool[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
    uint8_t fec_pool_old[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK][MAX_USER_PACKET_LENGTH];
    uint8_t *fec_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    uint8_t *fec_blocks_old[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    for (int i = 0; i < num_fec_blocks; ++i) {
        fec_blocks[i] = fec_pool[i];
        fec_blocks_old[i] = fec_pool_old[i];
    }
    srand(time(NULL));
    for (int i = 0; i < test_data_buff_size; i++)
        test_data[i] = (rand() % (254 - 0 + 1)) + 0;

    printf("\nTesting FEC encode\n");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < num_data_blocks; ++i) {
        data_blocks_old[i] = &test_data[i * packet_size];
    }
    fec_encode_old(packet_size, data_blocks_old, num_data_blocks, (unsigned char **) fec_blocks_old, num_fec_blocks);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("FEC old encode took %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (int i = 0; i < num_data_blocks; ++i) {
        data_blocks[i] = &test_data[i * packet_size];
    }
    fec_encode(packet_size, data_blocks, num_data_blocks, (unsigned char **) fec_blocks, num_fec_blocks);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("FEC new encode took %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));

    int invalid = 0;
    for (int i = 0; i < num_fec_blocks; i++) {
        if (memcmp(fec_pool_old[i], fec_pool[i], packet_size) != 0) {
            invalid = 1;
        }
    }
    if (invalid == 0) {
        printf(KGRN"Both FEC encode functions produce equal results!\n"KNRM);
    } else {
        printf(KRED "FEC encode results not byte-equal!\n"KNRM);
    }


    unsigned int fec_block_nos[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    unsigned int erased_blocks[MAX_DATA_OR_FEC_PACKETS_PER_BLOCK];
    int number_applied_fec_packs = 1;
    int index_erased_data_block = 1; // second data packet is missing
    int index_repairing_fec_block = 0; // first FEC packet used to repair data
    erased_blocks[0] = index_erased_data_block;
    fec_block_nos[0] = index_repairing_fec_block;
    uint8_t erased_data[packet_size];

    memcpy(erased_data, data_blocks_old[index_erased_data_block], packet_size);
    memset(data_blocks[index_erased_data_block], 0, packet_size);     // create missing data packet
    memset(data_blocks_old[index_erased_data_block], 0, packet_size); // create missing data packet

    printf("\nTesting FEC decode\n");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    fec_decode_old((unsigned int) packet_size, data_blocks_old, num_data_blocks, fec_blocks_old, fec_block_nos,
                   erased_blocks, number_applied_fec_packs);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("Old FEC decode took %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    if (memcmp(data_blocks_old[index_erased_data_block], erased_data, packet_size) == 0) {
        printf(KGRN"Old FEC decode works!\n"KNRM);
    } else {
        printf(KRED "Old FEC decode produces wrong result!\n"KNRM);
    }

    memcpy(erased_data, data_blocks[index_erased_data_block], packet_size);
    memset(data_blocks[index_erased_data_block], 0, packet_size);     // create missing data packet
    memset(data_blocks_old[index_erased_data_block], 0, packet_size); // create missing data packet

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    fec_decode(packet_size, data_blocks, num_data_blocks, fec_blocks, fec_block_nos,
               erased_blocks, number_applied_fec_packs);
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("New FEC decode took %.02f microseconds\n", TimeSpecToUSeconds(&end_time) - TimeSpecToUSeconds(&start_time));
    if (memcmp(data_blocks[index_erased_data_block], erased_data, packet_size) == 0) {
        printf(KGRN"New FEC decode works!\n"KNRM);
    } else {
        printf(KRED "New FEC decode produces wrong result!\n"KNRM);
    }

    return 0;
}