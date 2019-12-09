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

#pragma once

#include <stdint.h>

#define REC_BUFF_SIZE 819200    // ~6Mbit local RAM buffer
// values need to be reset by corresponding thread before they hit their limit
// count represents bytes per file
extern volatile uint32_t receive_count, write_count;   // allows for 4GB video files
uint8_t rec_buff[REC_BUFF_SIZE];
extern volatile int recorder_running;

void recorder(void);