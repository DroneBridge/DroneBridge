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


#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>
#include "linux_aoa.h"
#include "../common/db_common.h"

bool abort_aoa_init = false;
uint8_t raw_usb_msg_buff[DB_AOA_MAX_MSG_LENGTH] = {0};
db_usb_msg_t *usb_msg = (db_usb_msg_t *) raw_usb_msg_buff;

uint16_t db_usb_max_packet_size = 512 - DB_AOA_HEADER_LENGTH;

u_int16_t get_db_usb_max_packet_size() {
    return db_usb_max_packet_size;
}

bool is_accessory_device(libusb_device *device, db_accessory_t *accessory) {
    struct libusb_device_descriptor desc = {0};
    int rc = libusb_get_device_descriptor(device, &desc);
    if (rc == 0) {
        if (desc.idVendor == AOA_ACCESSORY_VID) {
            switch (desc.idProduct) {
                case AOA_ACCESSORY_PID:
                case AOA_ACCESSORY_ADB_PID:
                case AOA_AUDIO_PID:
                case AOA_AUDIO_ADB_PID:
                case AOA_ACCESSORY_AUDIO_PID:
                case AOA_ACCESSORY_AUDIO_ADB_PID:
                    accessory->pid = desc.idProduct;
                    accessory->vid = AOA_ACCESSORY_VID;
                    return true;
                default:
                    return false;
            }
        }
    }
    return false;
}

/**
 * Searches for a connected device in android accessory mode and opens a connection
 * @param accessory
 * @return 1 on success, 0 on failure
 */
int connect_to_device_in_accessory_mode(db_accessory_t *accessory) {
    // check for already present accessories
    libusb_device **device_list;
    ssize_t cnt = libusb_get_device_list(NULL, &device_list);

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device *device = device_list[i];
        if (is_accessory_device(device, accessory)) {
            int ret = libusb_open(device, &accessory->handle);
            if (ret != 0 || accessory->handle == NULL) {
                fprintf(stderr, "AOA_USB: ERROR - Unable to open connected device in android accessory mode: %s\n",
                        libusb_error_name(ret));
                libusb_free_device_list(device_list, 1);
                return -1;
            }
            LOG_SYS_STD(LOG_INFO, "AOA_USB: Detected device in accessory mode: %4.4x:%4.4x\n", AOA_ACCESSORY_VID,
                        accessory->pid);

//            if ((ret = libusb_set_configuration(accessory->handle, 0)) != 0)
//                fprintf(stderr, "--> Error setting device configuration %s\n", libusb_error_name(ret));

            struct libusb_config_descriptor *config_descriptor;
            ret = libusb_get_active_config_descriptor(device, &config_descriptor);
            if (ret != 0)
                fprintf(stderr, "AOA_USB: ERROR - getting active config desc. %s\n", libusb_error_name(ret));
            LOG_SYS_STD(LOG_INFO, "AOA_USB:\tGot %i interfaces\n", config_descriptor->bNumInterfaces);
            db_usb_max_packet_size =
                    config_descriptor->interface[0].altsetting->endpoint[0].wMaxPacketSize - DB_AOA_HEADER_LENGTH - 1;
            LOG_SYS_STD(LOG_INFO, "AOA_USB:\tMax packet size is %i bytes\n", db_usb_max_packet_size);

            ret = libusb_claim_interface(accessory->handle, 0);
            if (ret != 0)
                fprintf(stderr, "AOA_USB: ERROR - claiming AOA interface: %s\n", libusb_error_name(ret));

            libusb_free_config_descriptor(config_descriptor);
            libusb_free_device_list(device_list, 1);
            return 1;
        }
    }
    libusb_free_device_list(device_list, 1);
    LOG_SYS_STD(LOG_INFO, "AOA_USB: No device in accessory mode found\n");
    return 0;
}


/**
 * Checks if specified device supports android open accessory protocol
 * @param usb_dev
 * @param db_acc
 * @return
 */
int supports_aoa(libusb_device *usb_dev, db_accessory_t *db_acc) {
    uint8_t buffer[2];
    libusb_device_handle *dev_handle;

    int ret = libusb_open(usb_dev, &dev_handle);
    if (ret != 0 || dev_handle == NULL) {
        return 0;
    }

    // Now asking if device supports Android Open Accessory protocol
    ret = libusb_control_transfer(dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR, AOA_GET_PROTOCOL,
                                  0, 0, buffer, sizeof(buffer), 2000);
    if (ret == 0) {
        fprintf(stderr, "AOA_USB: ERROR - Could not get protocol: %s\n", libusb_error_name(ret));
        libusb_close(db_acc->handle);
        return 0;
    } else {
        uint32_t version_num = ((buffer[1] << 8) | buffer[0]);
        if (version_num <= 2 && version_num > 0) {
            db_acc->aoa_version = version_num;
            db_acc->handle = dev_handle;
            LOG_SYS_STD(LOG_INFO, "AOA_USB: Found device that supports AOA %d.0!\n", db_acc->aoa_version);
            usleep(10000);
            return 1;
        } else {
            libusb_close(db_acc->handle);
            return 0;
        }
    }
}


void signal_callback(int signum) {
    abort_aoa_init = true;
}


/**
 * Discover USB devices that support android accessory.
 *
 * @param db_acc
 * @return
 */
int discover_compatible_devices(db_accessory_t *db_acc) {
    libusb_device **device_list;
    libusb_device *found_device = NULL;
    ssize_t cnt = libusb_get_device_list(NULL, &device_list);

    LOG_SYS_STD(LOG_INFO, "AOA_USB: Checking %zi USB devices\n", cnt);
    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device *device = device_list[i];
        if (supports_aoa(device, db_acc)) {
            found_device = device;  // found a device that supports android accessory
            break;
        }
    }
    libusb_free_device_list(device_list, 1);

    // open detected device to put it into accessory mode (next step. not in this function)
    if (found_device) {
        int ret = libusb_open(found_device, &db_acc->handle);
        if (ret != 0 || db_acc->handle == NULL) {
            fprintf(stderr, "AOA_USB: ERROR - Unable to open detected device: %s\n", libusb_error_name(ret));
            return -1;
        }
        return 1;
    }
    return 0;
}

/**
 * Init function for all USB communication
 * Blocking call.
 * Searches for usb devices in android accessory mode and opens them.
 * In case of no open device it will try to find a supported device (android phone) amongst the connected USB devices &
 * put the device into android accessory mode
 * @param db_acc
 * @return -1 on kill or failure, 1 on already connected
 */
int init_db_accessory(db_accessory_t *db_acc) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    memset(db_acc, 0, sizeof(db_accessory_t));
    action.sa_handler = signal_callback;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int ret = libusb_init(NULL);
    if (ret != 0) {
        fprintf(stderr, "AOA_USB: ERROR - Could not init libusb: %d\n", ret);
        return -1;
    }

    if (connect_to_device_in_accessory_mode(db_acc) < 1) {
        // No device in accessory mode connected. Search
        while (!abort_aoa_init) {
            usleep(1000000);
            int found_dev = discover_compatible_devices(db_acc);
            if (found_dev) {
                if (abort_aoa_init) return -1;
                LOG_SYS_STD(LOG_INFO, "AOA_USB: \tSending manufacturer identification: %s\n", DB_AOA_MANUFACTURER);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_MAN_ID, (uint8_t *) DB_AOA_MANUFACTURER,
                                            strlen(DB_AOA_MANUFACTURER) + 1,
                                            0) < 0) {
                    fprintf(stderr,
                            "\x1B[31m" "--> Error sending manufacturer information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);
                LOG_SYS_STD(LOG_INFO, "AOA_USB: \tSending model identification: %s\n", DB_AOA_MODEL_NAME);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_MOD_ID, (uint8_t *) DB_AOA_MODEL_NAME,
                                            strlen(DB_AOA_MODEL_NAME) + 1,
                                            0) < 0) {
                    fprintf(stderr,
                            "\x1B[31m" "AOA_USB: ERROR - sending model information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);
                LOG_SYS_STD(LOG_INFO, "AOA_USB:\tSending description: %s\n", DB_AOA_DESC);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_DSC_ID, (uint8_t *) DB_AOA_DESC, strlen(DB_AOA_DESC) + 1, 0) < 0) {
                    fprintf(stderr, "\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);
                LOG_SYS_STD(LOG_INFO, "AOA_USB:\tSending version information: %s\n", DB_AOA_VERSION);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_VER_ID, (uint8_t *) DB_AOA_VERSION, strlen(DB_AOA_VERSION) + 1,
                                            0) < 0) {
                    fprintf(stderr, "\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);
                LOG_SYS_STD(LOG_INFO, "AOA_USB:\tSending URL identification: %s\n", DB_AOA_URL);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_URL_ID, (uint8_t *) DB_AOA_URL, strlen(DB_AOA_URL) + 1, 0) < 0) {
                    fprintf(stderr, "\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);
                LOG_SYS_STD(LOG_INFO, "AOA_USB:\tSending serial number: %s\n", DB_AOA_VERSION);
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_SEND_IDENT, 0,
                                            AOA_STRING_SER_ID, (uint8_t *) DB_AOA_SER, strlen(DB_AOA_SER) + 1, 0) < 0) {
                    fprintf(stderr, "\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
                    continue;
                }
                usleep(10000);

                LOG_SYS_STD(LOG_INFO, "AOA_USB: Enabling accessory mode on device\n");
                if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                            AOA_START_ACCESSORY,
                                            0, 0, NULL, 0, 0) < 0) {
                    fprintf(stderr, "\x1B[31m" "--> Error enabling accessory mode on device" "\x1B[0m" "\n");
                    continue;
                }
                usleep(10000);
                if (db_acc->handle != NULL) {
                    int rett;
                    if ((rett = libusb_release_interface(db_acc->handle, 0)) < 0)
                        fprintf(stderr, "AOA_USB: Error releasing interface %s\n", libusb_error_name(rett));
                }
                libusb_close(db_acc->handle);

                usleep(100000);
                // Connect to accessory
                int tries = 10;
                while (tries--) {
                    if (connect_to_device_in_accessory_mode(db_acc) > 0)
                        return 1;  // success init
                    else if (!tries)
                        return -1;  // finally failed init
                    else
                        usleep(2000000);  // retry opening connection
                }
            }
        }
    }
    return 1;
}


/**
 * Return a pointer to the direct send buffer of the USB connection. Fill buffer and send using db_usb_send_zc() for a
 * zero copy send operation.
 * @return
 */
struct db_usb_msg_t *db_usb_get_direct_buffer() {
    return (db_usb_msg_t *) raw_usb_msg_buff;
}


/**
 * Closes AOA device and libusb backend. Reset buffer. Call at exit
 *
 * @param db_acc
 */
void exit_close_aoa_device(db_accessory_t *db_acc) {
    memset(raw_usb_msg_buff, 0, DB_AOA_MAX_MSG_LENGTH);
    if (db_acc->handle != NULL) {
        int ret;
        if ((ret = libusb_release_interface(db_acc->handle, 0)) != 0)
            fprintf(stderr, "AOA_USB: ERROR - releasing interface: %s\n", libusb_error_name(ret));
        libusb_close(db_acc->handle);
        printf("AOA_USB: Devices closed!\n");
    }
    libusb_exit(NULL);
}
