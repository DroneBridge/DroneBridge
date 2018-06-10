# This file is part of DroneBridge licenced under Apache Licence 2
# https://github.com/seeul8er/DroneBridge/
# Created by Wolfgang Christl

import json
import configparser
import binascii
from itertools import chain
import os

tag = 'DB_COMM_MESSAGE: '
PATH_DRONEBRIDGE_GROUND_SETTINGS = "/boot/DroneBridgeGround.ini"
PATH_DRONEBRIDGE_AIR_SETTINGS = "/boot/DroneBridgeAir.ini"
PATH_WBC_SETTINGS = "/boot/wifibroadcast-1.txt"

# Used with general settings requests
wbc_settings_blacklist = ["TXMODE", "MAC_RX[0]", "FREQ_RX[0]", "MAC_RX[1]", "FREQ_RX[1]", "MAC_RX[2]", "FREQ_RX[2]",
                          "MAC_RX[3]", "FREQ_RX[3]", "MAC_TX[0]", "FREQ_TX[0]", "MAC_TX[1]", "FREQ_TX[1]",
                          "WIFI_HOTSPOT_NIC", "RELAY", "RELAY_NIC", "RELAY_FREQ", "QUIET", "FREQSCAN",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND", "TELEMETRY_OUTPUT_SERIALPORT_GROUND",
                          "FC_RC_BAUDRATE", "FC_RC_SERIALPORT", "TELEMETRY_UPLINK", "FC_MSP_SERIALPORT",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE", "TELEMETRY_OUTPUT_SERIALPORT_GROUND_BAUDRATE"]
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
    complete_response['type'] = 'settingsresponse'
    complete_response['response'] = loaded_json['request']
    complete_response['origin'] = origin
    complete_response['id'] = loaded_json['id']
    if loaded_json['request'] == 'db':
        if 'settings' in loaded_json:
            complete_response = read_dronebridge_settings(complete_response, origin, True, loaded_json['settings'])
        else:
            complete_response = read_dronebridge_settings(complete_response, origin, False, None)
    elif loaded_json['request'] == 'wbc':
        if 'settings' in loaded_json:
            complete_response = read_wbc_settings(complete_response, True, loaded_json['settings'])
        else:
            complete_response = read_wbc_settings(complete_response, False, None)
    response = json.dumps(complete_response)
    crc32 = binascii.crc32(str.encode(response))
    return response.encode()+crc32.to_bytes(4, byteorder='little', signed=False)


def new_settingschangesuccess_message(origin, new_id):
    """returns a settings change success message"""
    command = json.dumps({'destination': 4, 'type': 'settingssuccess', 'origin': origin, 'id': new_id})
    crc32 = binascii.crc32(str.encode(command))
    return command.encode()+crc32.to_bytes(4, byteorder='little', signed=False)


def change_settings_wbc(loaded_json, origin):
    try:
        with open(PATH_WBC_SETTINGS, 'r+') as file:
            lines = file.readlines()
            for key in loaded_json['settings']:
                for index, line in enumerate(lines):
                    if line.startswith(key+"="):
                        lines[index] = key+"="+loaded_json['settings'][key]+"\n"
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
        if origin == 'groundstation':
            section = 'Ground'
            filepath = PATH_DRONEBRIDGE_GROUND_SETTINGS
        elif origin == 'drone':
            section = 'Air'
            filepath = PATH_DRONEBRIDGE_AIR_SETTINGS
        with open(filepath, 'r+') as file:
            lines = file.readlines()
            for key in loaded_json['settings'][section]:
                for index, line in enumerate(lines):
                    if line.startswith(key+"="):
                        lines[index] = key+"="+loaded_json['settings'][section][key]+"\n"
            file.seek(0, 0)
            for line in lines:
                file.write(line)
            file.truncate()
            file.flush()
            os.fsync(file.fileno())
    except Exception as ex:
        print("Error writing db settings: "+str(ex))
        return False
    return True


def change_settings(loaded_json, origin):
    """takes a settings change request - executes it - returns a encoded settings change success message"""
    worked = False
    if loaded_json['change'] == 'db':
        worked = change_settings_db(loaded_json, origin)
    elif loaded_json['change'] == 'wbc':
        worked = change_settings_wbc(loaded_json, origin)
    if worked:
        return new_settingschangesuccess_message(origin, loaded_json['id'])
    else:
        return "error_settingschange".encode()


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
    if origin == 'groundstation':
        config.read(PATH_DRONEBRIDGE_GROUND_SETTINGS)
        section = 'GROUND'
        comm_ident = 'Ground'
    elif origin == 'drone':
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


def read_wbc_settings(response_header, specific_request, requestet_settings):
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
        lines = chain(('['+virtual_section+']',), lines)
        config.read_file(lines)

    if specific_request:
        for requested_set in requestet_settings['wbc']:
            if requested_set in config[virtual_section]:
                    settings[requested_set] = config.get(virtual_section, requested_set)
    else:
        for key in config[virtual_section]:
            if key not in wbc_settings_blacklist:
                settings[key] = config.get(virtual_section, key)

    response_header['settings'] = settings
    return response_header


def remove_first_line(filepath):
    with open(filepath, 'r') as f1:
        data = f1.read().splitlines(True)
    with open(filepath, 'w') as f2:
        f2.writelines(data[1:])


def comm_message_extract_info(message):
    alist = message.rsplit(b'}', 1)
    alist[0] = alist[0]+b'}'
    return alist


def check_package_good(extracted_info):
    """
    Checks the CRC32 of the message contained in extracted_info[1]
    :param extracted_info: extracted_info[0] is the message as json, extracted_info[1] are the four crc bytes
    :return: True if message has valid CRC32
    """
    if binascii.crc32(extracted_info[0]).to_bytes(4, byteorder='little', signed=False) == extracted_info[1]:
        return True
    print(tag+"Bad CRC!")
    return False
