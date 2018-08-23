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
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zconf.h>
#include "db_protocol.h"


/**
 * Copied form DroneBridge common lib
 * @return
 */
void db_rc_values_memory_init(db_rc_values *rc_values) {
    for(int i = 0; i < NUM_CHANNELS; i++) {
        rc_values->ch[i] = 1000;
    }
}

/**
 * Copied form DroneBridge common lib.
 * Opens the shared memory segment on read only mode.
 * Control module writes RC values in there.
 * @return
 */
db_rc_values *db_rc_values_memory_open(void) {
    int fd;
    for(;;) {
        fd = shm_open("/db_rc_values", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(fd > 0) {
            break;
        }
        printf("db_rc_values_memory_open: Waiting for init ... %s\n", strerror(errno));
        usleep((__useconds_t) 1e5);
    }

    if (ftruncate(fd, sizeof(db_rc_values)) == -1) {
        perror("db_rc_values_memory_open: ftruncate");
        exit(1);
    }

    void *retval = mmap(NULL, sizeof(db_rc_values), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (retval == MAP_FAILED) {
        perror("db_rc_values_memory_open: mmap");
        exit(1);
    }
    db_rc_values *tretval = (db_rc_values*)retval;
    db_rc_values_memory_init(tretval);
    return (db_rc_values*)retval;
}

int main(int argc, char *argv[]) {
    db_rc_values *rc_values = db_rc_values_memory_open();
    while (1){
        printf("CH1 %i\n", rc_values->ch[0]);
        sleep(1);
    }
}
