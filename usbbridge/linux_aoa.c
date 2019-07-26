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
#include "linux_aoa.h"


uint8_t raw_usb_msg_buff[DB_AOA_MAX_MSG_LENGTH] = {0};
db_usb_msg_t *usb_msg = (db_usb_msg_t *) raw_usb_msg_buff;

uint16_t max_packet_size = 512 - DB_AOA_HEADER_LENGTH;


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
                printf("DB_USB: ERROR - Unable to open connected device in android accessory mode: %s\n", libusb_error_name(ret));
                return -1;
            }
            printf("DB_USB: Detected device in accessory mode: %4.4x:%4.4x\n", AOA_ACCESSORY_VID, accessory->pid);

//            if ((ret = libusb_set_configuration(accessory->handle, 0)) != 0)
//                printf("--> Error setting device configuration %s\n", libusb_error_name(ret));

            struct libusb_config_descriptor *config_descriptor;
            ret = libusb_get_active_config_descriptor(device, &config_descriptor);
            if (ret != 0)
                printf("DB_USB: ERROR - getting active config desc. %s\n", libusb_error_name(ret));
            printf("DB_USB:\tGot %i interfaces\n", config_descriptor->bNumInterfaces);
            max_packet_size = config_descriptor->interface[0].altsetting->endpoint[0].wMaxPacketSize - DB_AOA_HEADER_LENGTH;
            printf("DB_USB:\tMax packet size is %i bytes\n", max_packet_size);

            ret = libusb_claim_interface(accessory->handle, 0);
            if (ret != 0)
                printf("DB_USB: ERROR - claiming AOA interface: %s\n", libusb_error_name(ret));

            libusb_free_config_descriptor(config_descriptor);
            libusb_free_device_list(device_list, 1);
            return 1;
        }
    }
    printf("DB_USB: No device in accessory mode found\n");
    return 0;
}


/**
 * Init libusb and open present android accessory devices.
 * @param accessory android accessory struct
 * @return
 *      1 in case we have an already opened accessory inited
 *      -1 in case of failure
 *      0 in case no device was found that is already in accessory mode
 */
int check_for_present_aoa_dev(db_accessory_t *accessory) {
    int ret = libusb_init(NULL);
    if (ret != 0) {
        printf("DB_USB: ERROR - Could not init libusb: %d\n", ret);
        return -1;
    }
    return connect_to_device_in_accessory_mode(accessory);
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
                                  0, 0, buffer, sizeof(buffer), 0);
    if (ret == 0) {
        printf("DB_USB: ERROR - Could not get protocol: %s\n", libusb_error_name(ret));
        return 0;
    } else {
        db_acc->aoa_version = ((buffer[1] << 8) | buffer[0]);
        db_acc->handle = dev_handle;
        printf("DB_USB: Found device that supports AOA %d.0!\n", db_acc->aoa_version);
        usleep(10000);
        return 1;
    }
}


/**
 * Discover USB devices that support android accessory
 * @param db_acc
 * @return
 */
int discover_compatible_devices(db_accessory_t *db_acc) {
    libusb_device **device_list;
    libusb_device *found_device = NULL;
    ssize_t cnt = libusb_get_device_list(NULL, &device_list);

    printf("DB_USB: Checking %zi USB devices\n", cnt);
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
            printf("DB_USB: ERROR - Unable to open detected device: %s\n", libusb_error_name(ret));
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
 * @return
 */
int init_db_accessory(db_accessory_t *db_acc) {
    usb_msg->ident[0] = 0x44;
    usb_msg->ident[0] = 0x42;
    usb_msg->ident[0] = 0x01;
    if (check_for_present_aoa_dev(db_acc) < 1) {
        int found_dev = discover_compatible_devices(db_acc);
        while (!found_dev) {
            usleep(1000000);
            found_dev = discover_compatible_devices(db_acc);
        }

        printf("DB_USB: \tSending manufacturer identification: %s\n", DB_AOA_MANUFACTURER);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_MAN_ID, (uint8_t *) DB_AOA_MANUFACTURER, strlen(DB_AOA_MANUFACTURER) + 1,
                                    0) < 0) {
            printf("\x1B[31m" "--> Error sending manufacturer information to android device \x1B[0m \n");
        }
        usleep(10000);
        printf("DB_USB: \tSending model identification: %s\n", DB_AOA_MODEL_NAME);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_MOD_ID, (uint8_t *) DB_AOA_MODEL_NAME, strlen(DB_AOA_MODEL_NAME) + 1,
                                    0) <
            0) {
            printf("\x1B[31m" "DB_USB: ERROR - sending model information to android device \x1B[0m \n");
        }
        usleep(10000);
        printf("DB_USB:\tSending description: %s\n", DB_AOA_DESC);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_DSC_ID, (uint8_t *) DB_AOA_DESC, strlen(DB_AOA_DESC) + 1, 0) < 0) {
            printf("\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
        }
        usleep(10000);
        printf("DB_USB:\tSending version information: %s\n", DB_AOA_VERSION);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_VER_ID, (uint8_t *) DB_AOA_VERSION, strlen(DB_AOA_VERSION) + 1, 0) < 0) {
            printf("\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
        }
        usleep(10000);
        printf("DB_USB:\tSending URL identification: %s\n", DB_AOA_URL);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_URL_ID, (uint8_t *) DB_AOA_URL, strlen(DB_AOA_URL) + 1, 0) < 0) {
            printf("\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
        }
        usleep(10000);
        printf("DB_USB:\tSending serial number: %s\n", DB_AOA_VERSION);
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, AOA_SEND_IDENT, 0,
                                    AOA_STRING_SER_ID, (uint8_t *) DB_AOA_SER, strlen(DB_AOA_SER) + 1, 0) < 0) {
            printf("\x1B[31m" "--> Error sending URL information to android device \x1B[0m \n");
        }
        usleep(10000);

        printf("DB_USB: Enabling accessory mode on device\n");
        if (libusb_control_transfer(db_acc->handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
                                    AOA_START_ACCESSORY,
                                    0, 0, NULL, 0, 0) < 0) {
            printf("\x1B[31m" "--> Error enabling accessory mode on device" "\x1B[0m" "\n");
            return -1;
        }
        usleep(10000);
        if (db_acc->handle != NULL)
            libusb_release_interface(db_acc->handle, 0);
        libusb_close(db_acc->handle);

        // Connect to accessory
        int tries = 10;
        while (tries--) {
            if (connect_to_device_in_accessory_mode(db_acc) > 0)
                break;
            else if (!tries)
                return -1;
            else
                usleep(2000000);
        }
    }
    return 1;
}


void db_usb_send_debug(db_accessory_t *db_acc) {
    int num_trans;
    uint16_t data_length = 64;
    uint8_t data[64] = {6};
    int ret = libusb_bulk_transfer(db_acc->handle, AOA_ACCESSORY_EP_OUT, data, data_length, &num_trans, 1000);
    if (ret != 0 && ret != LIBUSB_ERROR_TIMEOUT)  // ignore timeout since it is normal when sending to AOA?!
        printf("DB_USB: ERROR - sending data (%i sent): %i %s\n", num_trans, ret, libusb_error_name(ret));
    else
        printf("DB_USB: Wrote some %i bytes\n", num_trans);
}


void db_usb_receive_debug(db_accessory_t *db_acc) {
    int num_trans;
    uint16_t data_length = 512;
    uint8_t data[512] = {0};
    int ret = libusb_bulk_transfer(db_acc->handle, AOA_ACCESSORY_EP_IN, data, data_length, &num_trans, 1000);
    if (ret != 0)
        printf("DB_USB: ERROR - receiving data: %s\n", libusb_error_name(ret));
    else
        printf("DB_USB: Got some debug data %i\n", num_trans);
}

/**
 * A function to send data over the USB interface using the DroneBridge USB message format. Uses memcpy
 * @param db_acc
 * @return: 0 on success or LIB_USB_ERROR on error
 */
int db_usb_send(db_accessory_t *db_acc, uint8_t data[], uint16_t data_length, uint8_t port) {
    if (data_length < max_packet_size) {
        usb_msg->port = port;
        usb_msg->pay_lenght = data_length;
        memcpy(usb_msg->payload, data, data_length);
        int num_trans;
        int ret = libusb_bulk_transfer(db_acc->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
                (data_length + DB_AOA_HEADER_LENGTH), &num_trans, 1000);
        if(num_trans != (data_length + DB_AOA_HEADER_LENGTH))
            printf("DB_USB: ERROR: Did not send all data (%i/%i)\n", (data_length + DB_AOA_HEADER_LENGTH), num_trans);
        else
            return ret;
    } else
        printf("DB_USB: ERROR: Supplied payload is too big for send buffer\n");
    return -1;
}

/**
 * Zero copy sending of usb message buffer. Get buffer using db_usb_get_direct_buffer() and fill it
 * @param db_acc DroneBridge android accessory
 * @return libusb return value - 0 or TIMEOUT on success or <0 on failure
 */
int db_usb_send_zc(db_accessory_t *db_acc) {
    if (usb_msg->pay_lenght < max_packet_size) {
        int num_trans;
        return libusb_bulk_transfer(db_acc->handle, AOA_ACCESSORY_EP_OUT, raw_usb_msg_buff,
                                       (usb_msg->pay_lenght + DB_AOA_HEADER_LENGTH), &num_trans, 1000);
    } else
        printf("DB_USB: ERROR: Supplied payload is too big for send buffer\n");
}

/**
 * Receive data from android accessory
 * @param db_acc DroneBridge android accessory
 * @param buffer Receive buffer to be filled with data
 * @param buffer_size Size of supplied receive buffer
 * @return Number of received bytes on success or libusb error (< 0)
 */
int db_usb_receive(db_accessory_t *db_acc, uint8_t buffer[], uint16_t buffer_size) {
    int num_trans;
    int ret = libusb_bulk_transfer(db_acc->handle, AOA_ACCESSORY_EP_IN, buffer, buffer_size, &num_trans, 1000);
    if (ret != 0)
        return ret; //printf("DB_USB: ERROR: receiving data: %s\n", libusb_error_name(ret));
    else
        return num_trans;
}


/**
 * Return a pointer to the direct send buffer of the USB connection. Fill buffer and send using db_usb_send_zc() for a
 * zero copy send operation.
 * @return
 */
struct db_usb_msg_t *db_usb_get_direct_buffer(){
    return (db_usb_msg_t *) raw_usb_msg_buff;
}


/**
 * Closes AOA device and libusb backend. Call at exit
 * @param db_acc
 */
void exit_close_aoa_device(db_accessory_t *db_acc) {
    if (db_acc->handle != NULL) {
        int ret;
        if ((ret = libusb_release_interface(db_acc->handle, 0)) != 0)
            printf("DB_USB: ERROR - releasing interface: %s\n", libusb_error_name(ret));
        libusb_close(db_acc->handle);
        printf("DB_USB: Devices closed!\n");
    }
    libusb_exit(NULL);
}
