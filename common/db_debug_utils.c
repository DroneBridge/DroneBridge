//
// Created by cyber on 10.06.18.
//

#include <stdint-gcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "db_debug_utils.h"


void print_buffer(uint8_t buffer[], int num_bytes){
    for (int i = 0; i < num_bytes; ++i) {
        printf("%2x ", buffer[i]);
    }
    printf("\n");
}
