import argparse
import os
import sys
import time

from Chipset import is_atheros_card, is_realtek_card
import pyric.pyw as pyw
import pyric.utils.hardware as iwhw
from subprocess import Popen

from common_helpers import read_dronebridge_config, get_bit_rate, HOTSPOT_NIC, PI3_WIFI_NIC

PATH_DB_VERSION = "../db_version.txt"
COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'


def parse_args():
    parser = argparse.ArgumentParser(description='Sets up the entire DroneBridge system including wireless adapters')
    parser.add_argument('-g', action='store_true', dest='gnd',
                        help='setup adapters on the ground station - if not set setup adapters for the UAV')
    return parser.parse_args()


def main():
    parsedArgs = parse_args()
    SETUP_GND = False  # If true we are operating on the ground station else we are on the UAV (by .profile via Cam)
    config = read_dronebridge_config()
    if parsedArgs.gnd:
        SETUP_GND = True
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
    wifi_ap_if = config.get(GROUND, 'wifi_ap_if')
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
        if winterface_name is not HOTSPOT_NIC:  # Leave ap-if alone
            if freq_ovr == 'N':
                # Set one frequency for each adapter
                setup_card(winterface_name, freq, datarate)
            else:
                # Set adapter specific frequencies
                try:
                    setup_card(winterface_name, list_man_freqs[list_man_nics.index(winterface_name)], datarate)
                except ValueError:
                    print("Unknown adapter " + winterface_name + " - could not set it up.")


def setup_card(interface_name, frequency, data_rate=2):
    wifi_card = pyw.getcard(interface_name)
    driver_name = iwhw.ifdriver(interface_name)
    print("Setting " + interface_name + " " + driver_name + " " + str(frequency) + "MHz")
    pyw.down(wifi_card)
    pyw.modeset(wifi_card, 'monitor')
    if is_realtek_card(driver_name):
        # Other cards power settings are set via e.g. 'txpower_atheros 58' or 'txpower_ralink 0' (defaults)
        pyw.txset(wifi_card, 30, 'fixed')
    pyw.up(wifi_card)
    pyw.freqset(wifi_card, frequency)
    if is_atheros_card(driver_name):
        # for all other cards the transmission rate is set via the radiotap header
        set_bitrate(interface_name, data_rate)


def set_bitrate(interface_name, datarate):
    Popen(['iw dev ' + interface_name + ' set bitrates legacy-2.4 ' + get_bit_rate(datarate)], shell=True)


def waitfor_network_adapters(wifi_ap_if):
    """
    Wait for at least one network adapter to be detected by the OS. Adapters that will be used for the Wifi AP will be
    ignored
    :param wifi_ap_if: Name of the wifi adapter that will be the access point - this one will be ignored
    :return:
    """
    keep_waiting = True
    print("Waiting for wifi adapters to be detected ", end="")
    while keep_waiting:
        winterfaces = pyw.winterfaces()
        try:
            winterfaces.remove(PI3_WIFI_NIC)
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
        card = pyw.getcard(PI3_WIFI_NIC)
    else:
        card = pyw.getcard(interface)
    pyw.down(card)
    Popen(["ip link set " + card.dev + " name " + HOTSPOT_NIC], shell=True)
    pyw.up(card)
    pyw.inetset(card, '192.168.2.1')


def get_firmware_id():
    version_num = 0
    with open(PATH_DB_VERSION, 'r') as version_file:
        version_num = int(version_file.readline())
    return version_num


def check_root():
    """
    Checks if script is executed with root privileges and if not tries to run as root (requesting password)
    :return:
    """
    if os.geteuid() != 0:
        print("Script not started as root. Running sudo..")
        args = ['sudo', sys.executable] + sys.argv + [os.environ]
        # the next line replaces the currently-running process with the sudo
        os.execlpe('sudo', *args)


if __name__ == "__main__":
    check_root()
    main()
