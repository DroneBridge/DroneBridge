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

#ifndef DB_JOYSTIC_TEST_OPENTX_H
#define DB_JOYSTIC_TEST_OPENTX_H

#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */

#define MAX 32767

#define DEFAULT_OPENTX_CALIBRATION "jscal -s 8,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751,1,0,0,0,5534751,5534751 /dev/input/js0"
#define taranis_descriptor           "FrSky FrSky Taranis Joystick"

int opentx(int Joy_IF, char calibrate_comm[]);

#endif //DB_JOYSTIC_TEST_OPENTX_H
