import configparser
import argparse
import os
import pyric.pyw as pyw
from subprocess import Popen

PATH_DB_VERSION = "../db_version.txt"
PATH_CONFIG_FILE = os.path.join(os.sep, 'boot', os.sep, 'DroneBridgeConfig.ini')
COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'


def parse_args():
    parser = argparse.ArgumentParser(description='Sets up the entire DroneBridge system including wireless adapters')
    parser.add_argument('-o', action='store', dest='location', default='g',
                        help='set to \'g\' if executed on ground station or \'u\' if you want to set up UAV side')
    return parser.parse_args()


def main():
    parsedArgs = parse_args()
    SETUP_GND = True  # If true we are operating on the ground station else we are on the UAV (by .profile via Cam)
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(PATH_CONFIG_FILE)
    if parsedArgs.location == 'u':
        SETUP_GND = False
        print("Setting up DroneBridge v" + str(get_firmware_id() * 0.01) + " for UAV")
    else:
        print("Setting up DroneBridge v" + str(get_firmware_id() * 0.01) + " for ground station")
    print("Settings up network interfaces")
    # TODO: check for USB Stick?!
    # TODO: low voltage check on startup till we update OSD code
    setup_network_interfaces(SETUP_GND, config)


def setup_network_interfaces(SETUP_GND, config):
    # READ THE SETTINGS
    freq = config.getint(COMMON, 'freq')
    section = UAV
    if SETUP_GND:
        section = GROUND
        nic_3 = config.get(GROUND, 'nic_3')
        frq_3 = config.getint(GROUND, 'frq_3')
        nic_4 = config.get(GROUND, 'nic_4')
        frq_4 = config.getint(GROUND, 'frq_4')
    datarate = config.getint(section, 'datarate')
    freq_ovr = config.get(section, 'freq_ovr')
    nic_1 = config.get(section, 'nic_1')
    frq_1 = config.getint(section, 'frq_1')
    nic_2 = config.get(section, 'nic_2')
    frq_2 = config.getint(section, 'frq_2')

    # SET THE PARAMETERS
    # TODO: wait for network interfaces to become ready
    winterfaces = pyw.winterfaces()
    if freq_ovr == 'N':
        # Set one frequency for each adapter
        for winterface_name in winterfaces:
            if winterface_name is not 'wifihotspot0':  # Do not set internal wifi cards of Pi3 to monitor mode
                wifi_card = pyw.getcard(winterface_name)
                pyw.down(wifi_card)
                pyw.modeset(wifi_card, 'monitor')
                pyw.txset(wifi_card, 30, 'fixed')
                pyw.up(wifi_card)
                pyw.freqset(wifi_card, freq)
                set_bitrate(winterface_name, datarate)
    else:
        # Set adapter specific frequencies
        pass


def set_bitrate(interface_name, datarate):
    if datarate == 1:
        bit_rate = '6'
    elif datarate == 2:
        bit_rate = '11'
    elif datarate == 3:
        bit_rate = '12'
    elif datarate == 4:
        bit_rate = '18'
    elif datarate == 5:
        bit_rate = '24'
    else:
        bit_rate = '36'
    Popen(['iw dev ' + interface_name + ' set bitrates legacy-2.4 ' + bit_rate], shell=True)


def get_firmware_id():
    version_num = 0
    with open(PATH_DB_VERSION, 'r') as version_file:
        version_num = int(version_file.readline())
    return version_num


if __name__ == "__main__":
    main()
