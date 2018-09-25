import configparser
import argparse
import os
import time

from Chipset import isatheros_card, isrealtek_card
import pyric.pyw as pyw
import pyric.utils.hardware as iwhw
from subprocess import Popen

PATH_DB_VERSION = "../db_version.txt"
PATH_CONFIG_FILE = os.path.join(os.sep, 'boot', os.sep, 'DroneBridgeConfig.ini')
COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'

p3wifi_nic = 'intwifi0'
hotspotname = 'wifihotspot0'


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
    if SETUP_GND and config.get(GROUND, 'wifi_ap') == 'Y':
        setup_hotspot(config.get(GROUND, 'wifi_ap_if'))
    setup_network_interfaces(SETUP_GND, config)


def setup_network_interfaces(SETUP_GND, config):
    # READ THE SETTINGS
    freq = config.getint(COMMON, 'freq')
    section = UAV
    list_man_nics = [None] * 4
    list_man_freqs = [None] * 4
    if SETUP_GND:
        section = GROUND
        list_man_nics[2] = config.get(GROUND, 'nic_3')
        list_man_freqs[2] = config.getint(GROUND, 'frq_3')
        list_man_nics[3] = config.get(GROUND, 'nic_4')
        list_man_freqs[3] = config.getint(GROUND, 'frq_4')
    wifi_ap_if = config.get(section, 'wifi_ap_if')
    datarate = config.getint(section, 'datarate')
    freq_ovr = config.get(section, 'freq_ovr')
    list_man_nics[0] = config.get(section, 'nic_1')
    list_man_freqs[0] = config.getint(section, 'frq_1')
    list_man_nics[1] = config.get(section, 'nic_2')
    list_man_freqs[1] = config.getint(section, 'frq_2')

    # SET THE PARAMETERS
    waitfor_network_adapters(wifi_ap_if)
    winterfaces = pyw.winterfaces()
    for winterface_name in winterfaces:
        if winterface_name is not hotspotname:  # Leave ap-if alone
            if freq_ovr == 'N':
                # Set one frequency for each adapter
                setup_card(winterface_name, freq, datarate)
            else:
                # Set adapter specific frequencies
                try:
                    setup_card(winterface_name, list_man_freqs[list_man_nics.index(winterface_name)], datarate)
                except ValueError:
                    print("Unknown adapter " + winterface_name + " - could not set it up.")


def setup_card(interface_name, frequency, data_rate=3):
    wifi_card = pyw.getcard(interface_name)
    driver_name = iwhw.ifdriver(interface_name)
    print("Setting " + interface_name + " " + driver_name + " " + frequency + "MHz")
    pyw.down(wifi_card)
    pyw.modeset(wifi_card, 'monitor')
    if isrealtek_card(driver_name):
        # Other cards power settings are set via e.g. 'txpower_atheros 58' or 'txpower_ralink 0' (defaults)
        pyw.txset(wifi_card, 30, 'fixed')
    pyw.up(wifi_card)
    pyw.freqset(wifi_card, frequency)
    if isatheros_card(driver_name):
        # for all other cards the transmission rate is set via the radiotap header
        set_bitrate(interface_name, data_rate)


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


def waitfor_network_adapters(wifi_ap_if):
    keep_waiting = True
    print("Waiting for wifi adapters to be detected ", end="")
    while keep_waiting:
        winterfaces = pyw.winterfaces()
        try:
            winterfaces.remove(p3wifi_nic)
        except ValueError:
            pass
        try:
            winterfaces.remove(wifi_ap_if)
        except ValueError:
            pass
        if len(winterfaces) > 0:
            keep_waiting = False
            print("\n")
        else:
            print(".", end="")
            time.sleep(0.5)


def setup_hotspot(interface):
    if interface == 'internal':
        card = pyw.getcard(p3wifi_nic)
    else:
        card = pyw.getcard(interface)
    pyw.down(card)
    Popen(["ip link set " + card.dev + " name " + hotspotname], shell=True)
    pyw.up(card)
    pyw.inetset(card, '192.168.2.1')


def get_firmware_id():
    version_num = 0
    with open(PATH_DB_VERSION, 'r') as version_file:
        version_num = int(version_file.readline())
    return version_num


if __name__ == "__main__":
    main()
