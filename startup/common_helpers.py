import configparser
import os
from pathlib import Path

PATH_CONFIG_FILE = os.path.join(os.sep, "DroneBridge", "DroneBridgeConfig.ini")
PI3_WIFI_NIC = 'intwifi0'
HOTSPOT_NIC = 'wifihotspot0'

def read_dronebridge_config():
    """
    Reads the DroneBridge config file
    :return: A configparser object containing the config
    """
    config_file = Path(PATH_CONFIG_FILE)
    if config_file.exists():
        config = configparser.ConfigParser()
        config.optionxform = str
        config.read(PATH_CONFIG_FILE)
        return config
    else:
        print("Could not find config file at " + PATH_CONFIG_FILE)
        return None


def get_bit_rate(datarate_index):
    """
    Convert the data rate index given in the config file to the respective numeric data rate.

    :param datarate_index: data rate in Mbps
    :return: A string specifying the data rate
    """
    if datarate_index == 1:
        return '1'
    elif datarate_index == 2:
        return '2'
    elif datarate_index == 5.5:
        return '5.5'
    elif datarate_index == 6:
        return '6'
    elif datarate_index == 9:
        return '9'
    elif datarate_index == 11:
        return '11'
    elif datarate_index == 12:
        return '12'
    elif datarate_index == 18:
        return '18'
    elif datarate_index == 24:
        return '24'
    elif datarate_index == 36:
        return '36'
    elif datarate_index == 54:
        return '54'
    else:
        return '6'
