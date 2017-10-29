import json
import configparser
import binascii
from itertools import chain


# Creates JSON messages for DB Communication Protocol to be sent out


tag = 'DB_COMM_MESSAGE: '
PATH_DRONEBRIDGE_TX_SETTINGS = "/boot/DroneBridgeTX.ini"
PATH_DRONEBRIDGE_RX_SETTINGS = "/boot/DroneBridgeRX.ini"
PATH_WBC_SETTINGS = "/boot/wifibroadcast-1.txt"

# As we send it as a single frame we do not want the payload to be unnecessarily big. Only respond important settings
wbc_settings_blacklist = ["TXMODE", "MAC_RX[0]", "FREQ_RX[0]", "MAC_RX[1]", "FREQ_RX[1]", "MAC_RX[2]", "FREQ_RX[2]",
                          "MAC_RX[3]", "FREQ_RX[3]", "MAC_TX[0]", "FREQ_TX[0]", "MAC_TX[1]", "FREQ_TX[1]",
                          "WIFI_HOTSPOT_NIC", "RELAY", "RELAY_NIC", "RELAY_FREQ", "QUIET", "FREQSCAN",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND", "TELEMETRY_OUTPUT_SERIALPORT_GROUND",
                          "FC_RC_BAUDRATE", "FC_RC_SERIALPORT", "TELEMETRY_UPLINK", "FC_MSP_SERIALPORT",
                          "EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE", "TELEMETRY_OUTPUT_SERIALPORT_GROUND_BAUDRATE"]
db_settings_blacklist = ["ip_drone", "interface_selection", "interface_control", "interface_tel", "interface_video",
                         "interface_comm", "joy_cal"]


def new_settingsresponse_message(loaded_json, origin):
    """takes in a request - executes search for settings and creates a response as bytes"""
    complete_response = {}
    complete_response['destination'] = 4
    complete_response['type'] = 'settingsresponse'
    complete_response['response'] = loaded_json['request']
    complete_response['origin'] = origin
    complete_response['id'] = loaded_json['id']
    if loaded_json['request'] == 'dronebridge':
        complete_response = read_dronebridge_settings(complete_response, origin)
    elif loaded_json['request'] == 'wifibroadcast':
        complete_response = read_wbc_settings(complete_response)
    response = json.dumps(complete_response)
    crc32 = binascii.crc32(str.encode(response))
    return response.encode()+crc32.to_bytes(4, byteorder='little', signed=False)


"""returns a settings change success message"""
def new_settingschangesuccess_message(origin, new_id):
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
    except Exception as ex:
        print("Error writing wbc settings: " + str(ex))
        return False
    return True


def change_settings_db(loaded_json, origin):
    try:
        section = ''
        filepath = ''
        if origin=='groundstation':
            section = 'TX'
            filepath = PATH_DRONEBRIDGE_TX_SETTINGS
        elif origin == 'drone':
            section = 'RX'
            filepath = PATH_DRONEBRIDGE_RX_SETTINGS
        with open(filepath, 'r+') as file:
            lines = file.readlines()
            for key in loaded_json['settings'][section]:
                for index, line in enumerate(lines):
                    if line.startswith(key+"="):
                        lines[index] = key+"="+loaded_json['settings'][section][key]+"\n"
            file.seek(0, 0)
            for line in lines:
                file.write(line)
    except Exception as ex:
        print("Error writing wbc settings: "+str(ex))
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


def read_dronebridge_settings(response_header, origin):
    config = configparser.ConfigParser()
    config.optionxform = str
    section = ''
    settings = {}
    if origin == 'groundstation':
        config.read(PATH_DRONEBRIDGE_TX_SETTINGS)
        section = 'TX'
    elif origin == 'drone':
        config.read(PATH_DRONEBRIDGE_RX_SETTINGS)
        section = 'RX'

    for key in config[section]:
        if key not in db_settings_blacklist:
            settings[key] = config.get(section, key)

    response_header['settings'] = settings
    return response_header


def read_wbc_settings(response_header):
    virtual_section = 'root'
    settings = {}
    config = configparser.ConfigParser()
    config.optionxform = str
    with open(PATH_WBC_SETTINGS, 'r') as lines:
        lines = chain(('['+virtual_section+']',), lines)
        config.read_file(lines)

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
    if binascii.crc32(extracted_info[0]).to_bytes(4, byteorder='little', signed=False) == extracted_info[1]:
        return True
    print(tag+"Bad CRC!")
    return False