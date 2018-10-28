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

#ifdef USE_PI_INSTALL_PATH
#include "/root/dronebridge/common/shared_memory.h"
#else
#include "../../common/shared_memory.h"
#endif

int main(int argc, char *argv[]) {
    db_rc_values_t *rc_values = db_rc_values_memory_open();
    while (1){
        printf("CH1 %i\n", rc_values->ch[0]);
        sleep(1);
    }
}
