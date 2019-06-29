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

// requires read/write permission on usb udev device with vendor id: 0x18D1 (Google) -> edit udev rules accordingly
// https://unix.stackexchange.com/questions/44308/understanding-udev-rules-and-permissions-in-libusb

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "linux_aoa.h"


bool keeprunning = true;


void intHandler(int dummy) {
    keeprunning = false;
}


int main(int argc, char *argv[]) {
    db_accessory_t accessory;
    init_db_accessory(&accessory); // blocking

    exit_close_aoa_device(&accessory);
    return 0;
}