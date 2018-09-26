import configparser
import os

PATH_CONFIG_FILE = os.path.join(os.sep, 'boot', os.sep, 'DroneBridgeConfig.ini')
PI3_WIFI_NIC = 'intwifi0'
HOTSPOT_NIC = 'wifihotspot0'


def read_dronebridge_config():
    """
    Reads the DroneBridge config file
    :return: A configparser object containing the config
    """
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(PATH_CONFIG_FILE)
    return config


def get_bit_rate(datarate_index):
    """
    Convert the data rate index given in the config file to the respective numeric data rate.
    Eg. datarate_index 1 = 6Mbit
    :param datarate_index: Index of the data rate: 1=6Mbit, 2=11Mbit, 3=12Mbit, 4=18Mbit, 5=24Mbit, 6=36Mbit
    :return: A string specifying the data rate
    """
    if datarate_index == 1:
        return '6'
    elif datarate_index == 2:
        return '11'
    elif datarate_index == 3:
        return '12'
    elif datarate_index == 4:
        return '18'
    elif datarate_index == 5:
        return '24'
    else:
        return '36'
