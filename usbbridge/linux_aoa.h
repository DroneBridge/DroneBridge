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

#ifndef DRONEBRIDGE_LINUX_AOA_H
#define DRONEBRIDGE_LINUX_AOA_H

#include <stdint.h>
#include <libusb-1.0/libusb.h>


// according to Android Open Accessory protocol
#define AOA_GET_PROTOCOL		51
#define AOA_SEND_IDENT			52
#define AOA_START_ACCESSORY		53
#define AOA_AUDIO_SUPPORT	    58

#define AOA_ACCESSORY_VID		    0x18D1	// Google vendor ID

#define AOA_ACCESSORY_PID		    0x2D00	// accessory
#define AOA_ACCESSORY_ADB_PID		0x2D01	// accessory & adb
#define AOA_AUDIO_PID			    0x2D02	// audio
#define AOA_AUDIO_ADB_PID		    0x2D03	// audio & adb
#define AOA_ACCESSORY_AUDIO_PID		0x2D04	// accessory & audio
#define AOA_ACCESSORY_AUDIO_ADB_PID	0x2D05	// accessory & audio & adb

#define AOA_STRING_MAN_ID		0
#define AOA_STRING_MOD_ID		1
#define AOA_STRING_DSC_ID		2
#define AOA_STRING_VER_ID		3
#define AOA_STRING_URL_ID		4
#define AOA_STRING_SER_ID	    5

#define AOA_ACCESSORY_EP_IN		    0x81
#define AOA_ACCESSORY_EP_OUT	    0x02


// DroneBridge AOA specification
#define DB_AOA_MANUFACTURER     "DroneBridge"
#define DB_AOA_MODEL_NAME       "RaspberryPi"
#define DB_AOA_URL              "https://github.com/DroneBridge"
#define DB_AOA_VERSION          "1.0"
#define DB_AOA_DESC             "For gnd station to app communication via USB"
#define DB_AOA_SER              "0.6"

#define DB_AOA_MAX_MSG_LENGTH   1023
#define DB_AOA_HEADER_LENGTH    6
#define DB_AOA_MAX_PAY_LENGTH   1017


extern uint8_t raw_usb_msg_buff[DB_AOA_MAX_MSG_LENGTH];


typedef struct accessory_t {
    struct libusb_device_handle *handle;
    struct libusb_transfer *transfer;
    uint32_t aoa_version;
    uint16_t vid;
    uint16_t pid;
    char *device;
    char *manufacturer;
    char *model;
    char *description;
    char *version;
    char *url;
    char *serial;
} db_accessory_t;

typedef struct db_usb_msg_t {
    char ident[3];
    uint8_t port;
    uint16_t pay_lenght;
    uint8_t payload[DB_AOA_MAX_PAY_LENGTH];
} __attribute__((packed)) db_usb_msg_t ;


int init_db_accessory(db_accessory_t *db_acc);
int db_usb_send(db_accessory_t *db_acc, uint8_t data[], uint16_t data_length, uint8_t port);
void db_usb_send_debug(db_accessory_t *db_acc);
void db_usb_receive_debug(db_accessory_t *db_acc);
struct db_usb_msg_t *db_usb_get_direct_buffer();
int db_usb_send_zc(db_accessory_t *db_acc);
int db_usb_receive(db_accessory_t *db_acc, uint8_t buffer[], uint16_t buffer_size);
void exit_close_aoa_device(db_accessory_t *db_acc);

#endif //DRONEBRIDGE_LINUX_AOA_H
