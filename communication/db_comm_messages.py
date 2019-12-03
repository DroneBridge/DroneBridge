#
# This file is part of DroneBridgeLib: https://github.com/seeul8er/DroneBridge
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
import binascii
import configparser
import json
import os
# import RPi.GPIO as gp
from subprocess import call
from syslog import LOG_ERR

import evdev
from DroneBridge import DBDir

from DBCommProt import DBCommProt
from db_helpers import db_log

tag = 'DB_COMM_MESSAGE: '
PATH_DRONEBRIDGE_SETTINGS = "/DroneBridgeLib/DroneBridgeConfig.ini"
PATH_DB_VERSION = "/DroneBridgeLib/db_version.txt"

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


def process_db_comm_protocol(loaded_json: json, comm_direction: DBDir) -> bytes:
    """
    Execute the command given in the DroneBridgeLib communication packet

    :param loaded_json: The message to process
    :param comm_direction: The direction of the local program instance in which it is sending
    :return: correct response message
    """
    message = ""
    if loaded_json['type'] == DBCommProt.DB_TYPE_SETTINGS_REQUEST.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = new_settingsresponse_message(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = new_settingsresponse_message(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
    elif loaded_json['type'] == DBCommProt.DB_TYPE_SETTINGS_CHANGE.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = change_settings(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = change_settings(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
    elif loaded_json['type'] == DBCommProt.DB_TYPE_SYS_IDENT_REQUEST.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = create_sys_ident_response(loaded_json['id'], DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = create_sys_ident_response(loaded_json['id'], DBCommProt.DB_ORIGIN_UAV.value)
    elif loaded_json['type'] == DBCommProt.DB_TYPE_PING_REQUEST.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = new_ping_response_message(loaded_json['id'], DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = new_ping_response_message(loaded_json['id'], DBCommProt.DB_ORIGIN_UAV.value)
    elif loaded_json['type'] == DBCommProt.DB_TYPE_CAMSELECT.value:
        change_cam_selection(loaded_json['cam'])
        message = new_ack_message(DBCommProt.DB_ORIGIN_UAV.value, loaded_json['id'])
    elif loaded_json['type'] == DBCommProt.DB_TYPE_ADJUSTRC.value:
        normalize_jscal_axis(loaded_json['device'])
        message = new_ack_message(DBCommProt.DB_ORIGIN_GND.value, loaded_json['id'])
    elif loaded_json['type'] == DBCommProt.DB_TYPE_PARAM_REQ.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = new_settings_param_response(loaded_json['id'], DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = new_settings_param_response(loaded_json['id'], DBCommProt.DB_ORIGIN_UAV.value)
    elif loaded_json['type'] == DBCommProt.DB_TYPE_SECTION_REQ.value:
        if comm_direction == DBDir.DB_TO_UAV:
            message = new_settings_section_response(loaded_json['id'], DBCommProt.DB_ORIGIN_GND.value)
        else:
            message = new_settings_section_response(loaded_json['id'], DBCommProt.DB_ORIGIN_UAV.value)
    else:
        if comm_direction == DBDir.DB_TO_UAV:
            message = new_error_response_message('unsupported message type', DBCommProt.DB_ORIGIN_GND.value,
                                                 loaded_json['id'])
        else:
            message = new_error_response_message('unsupported message type', DBCommProt.DB_ORIGIN_UAV.value,
                                                 loaded_json['id'])
        db_log("DB_COMM_PROTO: Unknown message type", ident=LOG_ERR)
    return message


def new_settings_param_response(loaded_json: json, origin: int) -> bytes:
    """
    Return a message with a list of all changeable setting parameters without their values

    :param loaded_json:
    :param origin: is this a response of drone or ground station
    :return: message with a list of all changeable setting parameters without their values
    """
    return new_error_response_message("Section parameter request not supported in this version", origin,
                                      loaded_json['id'])


def new_settings_section_response(loaded_json: json, origin: int) -> bytes:
    """
    Return a message with a list of all changeable setting parameters without their values

    :param loaded_json:
    :param origin: is this a response of drone or ground station
    :return: message with a list of all changeable setting parameters without their values
    """
    return new_error_response_message("Section request not supported in this version", origin, loaded_json['id'])


def new_settingsresponse_message(loaded_json: json, origin: int) -> bytes:
    """
    takes in a request - executes search for settings and creates a response as bytes

    :param loaded_json:
    :param origin: is this a response of drone or ground station
    :return: a complete response packet as bytes
    """
    complete_response = {}
    complete_response['destination'] = DBCommProt.DB_DST_GCS.value
    complete_response['type'] = DBCommProt.DB_TYPE_SETTINGS_RESPONSE.value
    complete_response['response'] = loaded_json['request']
    complete_response['origin'] = origin
    complete_response['id'] = loaded_json['id']
    if loaded_json['request'] == DBCommProt.DB_REQUEST_TYPE_DB.value:
        if 'settings' in loaded_json:
            complete_response = read_dronebridge_settings(complete_response, True, loaded_json)  # can return None
        else:
            complete_response = read_dronebridge_settings(complete_response, False, None)  # can return None
    elif loaded_json['request'] == DBCommProt.DB_REQUEST_TYPE_WBC.value:
        db_log("DB_COMM_PROTO: ERROR - WBC settings read unsupported!", ident=LOG_ERR)
        return new_error_response_message("WBC settings read unsupported", origin, loaded_json['id'])
    if complete_response is None:
        return new_error_response_message("Could not read DroneBridgeLib config", origin, loaded_json['id'])
    response = json.dumps(complete_response)
    crc32 = binascii.crc32(str.encode(response))
    return response.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_settingschangesuccess_message(origin: int, new_id: int) -> bytes:
    """returns a settings change success message"""
    command = json.dumps({'destination': DBCommProt.DB_DST_GCS.value, 'type': DBCommProt.DB_TYPE_SETTINGS_SUCCESS.value, 'origin': origin,
                          'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_ack_message(origin: int, new_id: int) -> bytes:
    """returns a ACK message"""
    command = json.dumps({'destination': DBCommProt.DB_DST_GCS.value, 'type': DBCommProt.DB_TYPE_ACK.value, 'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_ping_response_message(request_id: int, origin: int) -> bytes:
    """returns a ping response message"""
    command = json.dumps({'destination': DBCommProt.DB_DST_GCS.value, 'type': DBCommProt.DB_TYPE_PING_RESPONSE.value,
                          'origin': origin, 'id': request_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def new_error_response_message(error_message: str, origin: int, new_id: int) -> bytes:
    """returns a error response message"""
    command = json.dumps({'destination': DBCommProt.DB_DST_GCS.value, 'type': DBCommProt.DB_TYPE_ERROR.value, 'message': error_message,
                          'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def change_settings_db(loaded_json: json) -> bool:
    try:
        with open(PATH_DRONEBRIDGE_SETTINGS, 'r+') as file:
            lines = file.readlines()
            for section in loaded_json['settings']:
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
        db_log(f"DB_COMM_PROTO: Error writing DroneBridgeLib settings: {ex}", ident=LOG_ERR)
        return False
    return True


def change_settings(loaded_json: json, origin: int) -> bytes:
    """takes a settings change request - executes it - returns a encoded settings change success message"""
    worked = False
    if loaded_json['change'] == DBCommProt.DB_REQUEST_TYPE_DB.value:
        worked = change_settings_db(loaded_json)
    elif loaded_json['change'] == DBCommProt.DB_REQUEST_TYPE_WBC.value:
        db_log("DB_COMM_PROTO: Error - WBC settings change not supported", ident=LOG_ERR)
        worked = False
    if worked:
        return new_settingschangesuccess_message(origin, loaded_json['id'])
    else:
        return new_error_response_message('Could not change settings', origin, loaded_json['id'])


def get_firmware_id() -> int:
    firmware_id = 0
    with open(PATH_DB_VERSION, 'r') as version_file:
        firmware_id = int(version_file.readline())
    return firmware_id


def create_sys_ident_response(requested_id: int, origin: int) -> bytes:
    command = json.dumps({'destination': DBCommProt.DB_DST_GCS.value, 'type': DBCommProt.DB_TYPE_SYS_IDENT_RESPONSE.value, 'origin': origin,
                          'HID': DBCommProt.DB_HWID_PI, 'FID': get_firmware_id(), 'id': requested_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode() + crc32.to_bytes(4, byteorder='little', signed=False)


def read_dronebridge_settings(response_header: dict, specific_request: bool, requested_settings: json) -> json or None:
    """
    Read settings from file and create a valid packet

    :param response_header: Everything but the settings part of the message as a dict
    :param specific_request: Is it a general or specific settings request: True|False
    :param requested_settings: A request json
    :return: The complete json with settings
    """
    config = configparser.ConfigParser()
    config.optionxform = str
    response_settings = {}  # settings object that gets sent
    config.read(PATH_DRONEBRIDGE_SETTINGS)
    if not config.read(PATH_DRONEBRIDGE_SETTINGS):
        db_log("DB_COMM_PROTO: Error reading DroneBridgeLib config", LOG_ERR)
        return None

    if specific_request:
        for section in requested_settings['settings']:
            temp_dict = {}
            for requested_setting in requested_settings['settings'][section]:
                if requested_setting in config[section]:
                    temp_dict[requested_setting] = config.get(section, requested_setting)
            response_settings[section] = temp_dict
    else:
        for section in requested_settings['settings']:
            temp_dict = {}
            for requested_setting in requested_settings['settings'][section]:
                if requested_setting in config[section]:
                    if requested_setting not in db_settings_blacklist:
                        temp_dict[requested_setting] = config.get(section, requested_setting)
            response_settings[section] = temp_dict

    response_header['settings'] = response_settings
    return response_header


def init_cam_gpios():
    return
    # gp.setwarnings(False)
    # gp.setmode(gp.BOARD)
    #
    # gp.setup(7, gp.OUT)
    # gp.setup(11, gp.OUT)
    # gp.setup(12, gp.OUT)
    #
    # gp.setup(15, gp.OUT)
    # gp.setup(16, gp.OUT)
    # gp.setup(21, gp.OUT)
    # gp.setup(22, gp.OUT)
    #
    # gp.output(11, True)
    # gp.output(12, True)
    # gp.output(15, True)
    # gp.output(16, True)
    # gp.output(21, True)
    # gp.output(22, True)


def change_cam_selection(camera_index):
    return
    # if camera_index == 0:
    #     gp.output(7, False)
    #     gp.output(11, False)
    #     gp.output(12, True)
    # elif camera_index == 1:
    #     gp.output(7, True)
    #     gp.output(11, False)
    #     gp.output(12, True)
    # elif camera_index == 2:
    #     gp.output(7, False)
    #     gp.output(11, True)
    #     gp.output(12, False)
    # elif camera_index == 3:
    #     gp.output(7, True)
    #     gp.output(11, True)
    #     gp.output(12, False)


def normalize_jscal_axis(device="/dev/input/js0"):
    """
    Reads the raw min and max values that the RC-HID will send to the ground station and calculates the calibration
    parameters to that the full range is used with no dead zone. The calibration is stored via "jscal-store".

    .. note:: This function does not calibrate the joystick! The user needs to calibrate the RC itself. This function just
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
            center_value = int((minimum + maximum) / 2)
            correction_coeff_min = int(536854528 / (maximum - center_value))
            correction_coeff_max = int(536854528 / (maximum - center_value))
            calibration_string = calibration_string + ",1,0," + str(center_value) + "," + str(center_value) + "," \
                                 + str(correction_coeff_min) + "," + str(correction_coeff_max)
        db_log("DB_COMM_PROTO: Calibrating:")
        db_log(calibration_string)
        call(["jscal", device, "-s", calibration_string])
        db_log("DB_COMM_PROTO: Saving calibration")
        call(["jscal-store", device])


def remove_first_line(filepath):
    with open(filepath, 'r') as f1:
        data = f1.read().splitlines(True)
    with open(filepath, 'w') as f2:
        f2.writelines(data[1:])


def parse_comm_message(raw_data_encoded: bytes) -> None or json:
    extracted_info = comm_message_extract_info(raw_data_encoded)  # returns json bytes [0] and crc bytes [1]
    try:
        loaded_json = json.loads(extracted_info[0].decode())
        if not comm_crc_correct(extracted_info):  # Check CRC
            db_log("DB_COMM_PROTO: Communication message: invalid CRC", ident=LOG_ERR)
            return None
        return loaded_json
    except UnicodeDecodeError:
        db_log("DB_COMM_PROTO: Invalid command: Could not decode json message", ident=LOG_ERR)
        return None
    except ValueError:
        db_log("DB_COMM_PROTO: ValueError on decoding json", ident=LOG_ERR)
        return None


def comm_message_extract_info(message: bytes) -> list:
    alist = message.rsplit(b'}', 1)
    alist[0] = alist[0] + b'}'
    return alist


def comm_crc_correct(extracted_info: list) -> bool:
    """
    Checks the CRC32 of the message contained in extracted_info[1]

    :param extracted_info: extracted_info[0] is the message as json, extracted_info[1] are the four crc bytes
    :return: True if message has valid CRC32
    """
    if binascii.crc32(extracted_info[0]).to_bytes(4, byteorder='little', signed=False) == extracted_info[1]:
        return True
    return False
