#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2018 Wolfgang Christl
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import json
import configparser
import binascii
from itertools import chain
import os
import RPi.GPIO as gp
from subprocess import call
import evdev


from DBCommProt import DBCommProt

tag = 'DB_COMM_MESSAGE: '
PATH_DRONEBRIDGE_GROUND_SETTINGS = "/boot/DroneBridgeGround.ini"
PATH_DRONEBRIDGE_AIR_SETTINGS = "/boot/DroneBridgeAir.ini"
PATH_WBC_SETTINGS = "/boot/wifibroadcast-1.txt"
PATH_DB_VERSION = "/boot/db_version.txt"

# Used with general settings requests
wbc_settings_blacklist = ["TXMODE", "MAC_RX[0]", "FREQ_RX[0]", "MAC_RX[1]", "FREQ_RX[1]", "MAC_RX[2]", "FREQ_RX[2]",
                          "MAC_RX[3]", "FREQ_RX[3]", "MAC_TX[0]", "FREQ_TX[0]", "MAC_TX[1]", "FREQ_TX[1]",
                          "WIFI_HOTSPOT_NIC", "RELAY", "RELAY_NIC", "RELAY_FREQ", "QUIET", "FREQSCAN",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND", "TELEMETRY_OUTPUT_SERIALPORT_GROUND",
                          "FC_RC_BAUDRATE", "FC_RC_SERIALPORT", "TELEMETRY_UPLINK", "FC_MSP_SERIALPORT",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE",
                          "TELEMETRY_OUTPUT_SERIALPORT_GROUND_BAUDRATE"]
db_settings_blacklist = ["ip_drone", "interface_selection", "interface_control", "interface_tel", "interface_video",
                         "interface_comm", "joy_cal"]


def new_settingsresponse_message(loaded_json, origin):
    """
    takes in a request - executes search for settings and creates a response as bytes
    :param loaded_json:
    :param origin: is this a response of drone or groundstation
    :return: a complete response packet as bytes
    """
    complete_response = {}
    complete_response['destination'] = 4
    complete_response['type'] = DBCommProt.DB_TYPE_SETTINGS_RESPONSE.value
    complete_response['response'] = loaded_json['request']
    complete_response['origin'] = origin
    complete_response['id'] = loaded_json['id']
    if loaded_json['request'] == DBCommProt.DB_REQUEST_TYPE_DB.value:
        if 'settings' in loaded_json:
            complete_response = read_dronebridge_settings(complete_response, origin, True, loaded_json['settings'])
        else:
            complete_response = read_dronebridge_settings(complete_response, origin, False, None)
    elif loaded_json['request'] == DBCommProt.DB_REQUEST_TYPE_WBC.value:
        if 'settings' in loaded_json:
            complete_response = read_wbc_settings(complete_response, True, loaded_json['settings'])
        else:
            complete_response = read_wbc_settings(complete_response, False, None)
    response = json.dumps(complete_response)
    crc32 = binascii.crc32(str.encode(response))
    return response.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_settingschangesuccess_message(origin, new_id):
    """returns a settings change success message"""
    command = json.dumps({'destination': 4, 'type': DBCommProt.DB_TYPE_SETTINGS_SUCCESS.value, 'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_ack_message(origin, new_id):
    """returns a ACK message"""
    command = json.dumps({'destination': 4, 'type': DBCommProt.DB_TYPE_ACK.value, 'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_ping_response_message(loaded_json, origin):
    """returns a ping response message"""
    command = json.dumps({'destination': 4, 'type': DBCommProt.DB_TYPE_PING_RESPONSE.value, 'origin': origin, 'id': loaded_json['id']})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_error_response_message(error_message, origin, new_id):
    """returns a error response message"""
    command = json.dumps({'destination': 4, 'type': DBCommProt.DB_TYPE_ERROR.value, 'message': error_message, 'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def change_settings_wbc(loaded_json, origin):
    try:
        with open(PATH_WBC_SETTINGS, 'r+') as file:
            lines = file.readlines()
            for key in loaded_json['settings']:
                for index, line in enumerate(lines):
                    if line.startswith(key + "="):
                        lines[index] = key + "=" + loaded_json['settings'][key] + "\n"
            file.seek(0, 0)
            for line in lines:
                file.write(line)
            file.truncate()
            file.flush()
            os.fsync(file.fileno())
    except Exception as ex:
        print("Error writing wbc settings: " + str(ex))
        return False
    return True


def change_settings_db(loaded_json, origin):
    try:
        section = ''
        filepath = ''
        if origin == DBCommProt.DB_ORIGIN_GND.value:
            section = 'Ground'
            filepath = PATH_DRONEBRIDGE_GROUND_SETTINGS
        elif origin == DBCommProt.DB_ORIGIN_UAV.value:
            section = 'Air'
            filepath = PATH_DRONEBRIDGE_AIR_SETTINGS
        with open(filepath, 'r+') as file:
            lines = file.readlines()
            for key in loaded_json['settings'][section]:
                for index, line in enumerate(lines):
                    if line.startswith(key + "="):
                        lines[index] = key + "=" + loaded_json['settings'][section][key] + "\n"
            file.seek(0, 0)
            for line in lines:
                file.write(line)
            file.truncate()
            file.flush()
            os.fsync(file.fileno())
    except Exception as ex:
        print("Error writing db settings: " + str(ex))
        return False
    return True


def change_settings(loaded_json, origin):
    """takes a settings change request - executes it - returns a encoded settings change success message"""
    worked = False
    if loaded_json['change'] == DBCommProt.DB_REQUEST_TYPE_DB.value:
        worked = change_settings_db(loaded_json, origin)
    elif loaded_json['change'] == DBCommProt.DB_REQUEST_TYPE_WBC.value:
        worked = change_settings_wbc(loaded_json, origin)
    if worked:
        return new_settingschangesuccess_message(origin, loaded_json['id'])
    else:
        return new_error_response_message('could not change settings', origin, loaded_json['id'])


def get_firmware_id():
    version_num = 0
    with open(PATH_DB_VERSION, 'r') as version_file:
        version_num = int(version_file.readline())
    return version_num


def create_sys_ident_response(loaded_json, origin):
    command = json.dumps({'destination': 4, 'type': DBCommProt.DB_TYPE_SYS_IDENT_RESPONSE.value, 'origin': origin,
                          'HID': 0, 'FID': get_firmware_id(), 'id': loaded_json['id']})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def change_settings_gopro(loaded_json):
    # TODO change GoPro settings
    pass


def read_dronebridge_settings(response_header, origin, specific_request, requestet_settings):
    """
    Read settings from file and create a valid packet
    :param response_header: Everything but the settings part of the message as a dict
    :param origin: Are we drone|groundstation
    :param specific_request: Is it a general or specific settings request: True|False
    :return: The complete json with settings
    """
    config = configparser.ConfigParser()
    config.optionxform = str
    section = ''  # section in the DroneBridge config file
    comm_ident = ''  # array descriptor in the settings request
    settings = {}  # settings object that gets sent
    if origin == DBCommProt.DB_ORIGIN_GND.value:
        config.read(PATH_DRONEBRIDGE_GROUND_SETTINGS)
        section = 'GROUND'
        comm_ident = 'Ground'
    elif origin == DBCommProt.DB_ORIGIN_UAV.value:
        config.read(PATH_DRONEBRIDGE_AIR_SETTINGS)
        section = 'AIR'
        comm_ident = 'Air'

    if specific_request:
        for requested_set in requestet_settings[comm_ident]:
            if requested_set in config[section]:
                settings[requested_set] = config.get(section, requested_set)
    else:
        for key in config[section]:
            if key not in db_settings_blacklist:
                settings[key] = config.get(section, key)

    response_header['settings'] = settings
    return response_header


def read_wbc_settings(response_header, specific_request, requested_settings):
    """
    Read settings from file and create a valid packet
    :param response_header: Everything but the settings part of the message as a dict
    :param specific_request: Is it a general or specific settings request: True|False
    :return: The complete json with settings
    """
    virtual_section = 'root'
    settings = {}
    config = configparser.ConfigParser()
    config.optionxform = str
    with open(PATH_WBC_SETTINGS, 'r') as lines:
        lines = chain(('[' + virtual_section + ']',), lines)
        config.read_file(lines)

    if specific_request:
        for requested_set in requested_settings['wbc']:
            if requested_set in config[virtual_section]:
                settings[requested_set] = config.get(virtual_section, requested_set)
    else:
        for key in config[virtual_section]:
            if key not in wbc_settings_blacklist:
                settings[key] = config.get(virtual_section, key)

    response_header['settings'] = settings
    return response_header


def init_cam_gpios():
    gp.setwarnings(False)
    gp.setmode(gp.BOARD)

    gp.setup(7, gp.OUT)
    gp.setup(11, gp.OUT)
    gp.setup(12, gp.OUT)

    gp.setup(15, gp.OUT)
    gp.setup(16, gp.OUT)
    gp.setup(21, gp.OUT)
    gp.setup(22, gp.OUT)

    gp.output(11, True)
    gp.output(12, True)
    gp.output(15, True)
    gp.output(16, True)
    gp.output(21, True)
    gp.output(22, True)


def change_cam_selection(camera_index):
    if camera_index == 0:
        gp.output(7, False)
        gp.output(11, False)
        gp.output(12, True)
    elif camera_index == 1:
        gp.output(7, True)
        gp.output(11, False)
        gp.output(12, True)
    elif camera_index == 2:
        gp.output(7, False)
        gp.output(11, True)
        gp.output(12, False)
    elif camera_index == 3:
        gp.output(7, True)
        gp.output(11, True)
        gp.output(12, False)


def normalize_jscal_axis(device="/dev/input/js0"):
    """
    Reads the raw min and max values that the RC-HID will send to the ground station and calculates the calibration
    parameters to that the full range is used with no dead zone. The calibration is stored via "jscal-store".
    NOTE: This function does not calibrate the joystick! The user needs to calibrate the RC itself. This function just
     tells the system to not use any dead zone and makes sure the full range of the output is being used
    :param device: The device descriptor
    :return:
    """
    devices = [evdev.InputDevice(path) for path in evdev.list_devices()]
    dev_capabilitys_list = devices[0].capabilities().get(3)
    if dev_capabilitys_list is not None:
        num_joystick_axis = len(dev_capabilitys_list)
        calibration_string = str(num_joystick_axis)
        for i in range(num_joystick_axis):
            absInfo = dev_capabilitys_list[i][1]
            minimum = absInfo[1]  # minimum value the RC will send for the first axis - raw value!
            maximum = absInfo[2]  # maximum value the RC will send for the first axis - raw value!
            center_value = int((minimum + maximum)/2)
            correction_coeff_min = int(536854528/(maximum - center_value))
            correction_coeff_max = int(536854528 / (maximum - center_value))
            calibration_string = calibration_string + ",1,0," + str(center_value) + "," + str(center_value) + "," \
                                 + str(correction_coeff_min) + "," + str(correction_coeff_max)
        print("Calibrating:")
        print(calibration_string)
        call(["jscal", device, "-s", calibration_string])
        print("Saving calibration")
        call(["jscal-store", device])


def remove_first_line(filepath):
    with open(filepath, 'r') as f1:
        data = f1.read().splitlines(True)
    with open(filepath, 'w') as f2:
        f2.writelines(data[1:])


def comm_message_extract_info(message):
    alist = message.rsplit(b'}', 1)
    alist[0] = alist[0] + b'}'
    return alist


def comm_crc_correct(extracted_info):
    """
    Checks the CRC32 of the message contained in extracted_info[1]
    :param extracted_info: extracted_info[0] is the message as json, extracted_info[1] are the four crc bytes
    :return: True if message has valid CRC32
    """
    if binascii.crc32(extracted_info[0]).to_bytes(4, byteorder='little', signed=False) == extracted_info[1]:
        return True
    print(tag + "Bad CRC!")
    return False
