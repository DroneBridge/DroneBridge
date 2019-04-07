import argparse
import os
import sys
import time

from Chipset import is_atheros_card, is_realtek_card
from CColors import CColors

import pyric.pyw as pyw
import pyric.utils.hardware as iwhw
from subprocess import Popen, DEVNULL

from common_helpers import read_dronebridge_config, get_bit_rate, HOTSPOT_NIC, PI3_WIFI_NIC

PATH_DB_VERSION = os.path.join(os.sep, "boot", "db_version.txt")
COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'

def parse_args():
    parser = argparse.ArgumentParser(description='Sets up the entire DroneBridge system including wireless adapters')
    parser.add_argument('-g', action='store_true', dest='gnd', default=False,
                        help='setup adapters on the ground station - if not set setup adapters for the UAV')
    return parser.parse_args()


def main():
    parsedArgs = parse_args()
    SETUP_GND = False  # If true we are operating on the ground station else we are on the UAV (by .profile via Cam)
    config = read_dronebridge_config()
    if parsedArgs.gnd:
        SETUP_GND = True
        print(CColors.OKGREEN + "Setting up DroneBridge v" + str(get_firmware_id() * 0.01) + " for ground station"
              + CColors.ENDC)
    else:
        print(CColors.OKGREEN + "Setting up DroneBridge v" + str(get_firmware_id() * 0.01) + " for UAV" + CColors.ENDC)
    print("Settings up network interfaces")
    # TODO: check for USB Stick?!
    # TODO: low voltage check on startup till we update OSD code
    setup_network_interfaces(SETUP_GND, config)  # blocks until interface becomes available
    if SETUP_GND and config.get(GROUND, 'wifi_ap') == 'Y':
        setup_hotspot(config.get(GROUND, 'wifi_ap_if'))
    if SETUP_GND and config.get(GROUND, 'eth_hotspot') == 'Y':
        setup_eth_hotspot()
    elif SETUP_GND:
        setup_eth_dhcp()


def setup_network_interfaces(SETUP_GND, config):
    # READ THE SETTINGS
    freq = config.getint(COMMON, 'freq')
    section = UAV
    list_man_nics = [None] * 4
    list_man_freqs = [None] * 4
    wifi_ap_blacklist = []

    if SETUP_GND:
        section = GROUND
        list_man_nics[2] = config.get(GROUND, 'nic_3')
        list_man_freqs[2] = config.getint(GROUND, 'frq_3')
        list_man_nics[3] = config.get(GROUND, 'nic_4')
        list_man_freqs[3] = config.getint(GROUND, 'frq_4')
        wifi_ap_blacklist.append(config.get(GROUND, 'wifi_ap_if'))
        wifi_ap_blacklist.append(HOTSPOT_NIC)
        wifi_ap_blacklist.append(PI3_WIFI_NIC)

    if config.has_option(COMMON, 'blacklist_ap')
        wifi_ap_blacklist.append(config.get(COMMON, 'blacklist_ap'))

    datarate = config.getint(section, 'datarate')
    freq_ovr = config.get(section, 'freq_ovr')
    list_man_nics[0] = config.get(section, 'nic_1')
    list_man_freqs[0] = config.getint(section, 'frq_1')
    list_man_nics[1] = config.get(section, 'nic_2')
    list_man_freqs[1] = config.getint(section, 'frq_2')

    print("Waiting for network adapters to become ready")
    # SET THE PARAMETERS
    waitfor_network_adapters(wifi_ap_blacklist)
    winterfaces = pyw.winterfaces()

    for winterface_name in winterfaces:
        if winterface_name not in wifi_ap_blacklist:  # Leave ap-if alone
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
    print("Settings up " + interface_name)
    wifi_card = pyw.getcard(interface_name)
    driver_name = iwhw.ifdriver(interface_name)
    print(CColors.OKGREEN + "Setting " + wifi_card.dev + " " + driver_name + " " + str(frequency) + " MHz" +
            " bitrate: " + get_bit_rate(data_rate) + " Mbps" + CColors.ENDC)
    pyw.up(wifi_card)
    if is_atheros_card(driver_name):
        # for all other cards the transmission rate is set via the radiotap header
        set_bitrate(interface_name, data_rate)
    if pyw.isup(wifi_card):
        print("\tdown...")
        pyw.down(wifi_card)
    print("\tmonitor...")
    pyw.modeset(wifi_card, 'monitor')
    if is_realtek_card(driver_name):
        # Other cards power settings are set via e.g. 'txpower_atheros 58' or 'txpower_ralink 0' (defaults)
        pyw.txset(wifi_card, 'fixed', 3000)
    pyw.up(wifi_card)
    print("\tfrequency...")
    pyw.freqset(wifi_card, frequency)
    print("\tMTU...")
    if is_realtek_card(driver_name):
        Popen(['ip link set dev ' + interface_name + ' mtu 1500'], shell=True).communicate()
    else:
        Popen(['ip link set dev ' + interface_name + ' mtu 2304'], shell=True).communicate()
    pyw.regset('DE')  # to allow channel 12 and 13 for hotspot
    rename_interface(interface_name)


def rename_interface(orig_interface_name):
    """
    Changes the name of the interface to its mac address
    :param orig_interface_name: The interface that you want to rename
    :return:
    """
    wifi_card = pyw.getcard(orig_interface_name)
    pyw.down(wifi_card)
    new_name = pyw.macget(wifi_card).replace(':', '')
    print("\trename...")
    Popen(['ip link set ' + orig_interface_name + ' name ' + new_name], shell=True).communicate()
    wifi_card = pyw.getcard(new_name)
    pyw.up(wifi_card)


def set_bitrate(interface_name, datarate):
    print("\tbitrate...")
    Popen(['iw dev ' + interface_name + ' set bitrates legacy-2.4 ' + get_bit_rate(datarate)], shell=True)


def waitfor_network_adapters(wifi_ap_blacklist=None):
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

        # Remove AP and internal interfaces for GND as well as blacklisted interfaces
        for wifi_if in wifi_ap_blacklist:
            try:
                winterfaces.remove(wifi_if)
            except ValueError:
                pass

        if len(winterfaces) > 0:
            keep_waiting = False
            print("\n")
        else:
            print(".", end="")
            time.sleep(1)
    time.sleep(2)


def setup_hotspot(interface):
    if interface == 'internal':
        card = pyw.getcard(PI3_WIFI_NIC)
    else:
        card = pyw.getcard(interface)
    pyw.down(card)
    Popen(["ip link set " + card.dev + " name " + HOTSPOT_NIC], shell=True)
    time.sleep(1)
    card = pyw.getcard(HOTSPOT_NIC)
    pyw.up(card)
    pyw.inetset(card, '192.168.2.1')
    Popen(["udhcpd -I 192.168.2.1 /etc/udhcpd-wifi.conf"], shell=True, close_fds=True, stdout=DEVNULL)
    Popen(["dos2unix -n /boot/apconfig.txt /tmp/apconfig.txt"], shell=True, close_fds=True, stdout=DEVNULL)
    Popen(["hostapd -B -d /tmp/apconfig.txt"], shell=True, close_fds=True, stdout=DEVNULL)
    print(CColors.OKGREEN + "Setup wifi hotspot: " + card.dev + " AP-IP: 192.168.2.1 " + CColors.ENDC)


def setup_eth_hotspot():
    print("Setting up ethernet hotspot")
    Popen(["ifconfig eth0 192.168.1.1 up"], shell=True)
    Popen(["udhcpd -I 192.168.1.1 /etc/udhcpd-eth.conf"], shell=True, close_fds=True, stdout=DEVNULL)


def setup_eth_dhcp():
    print("Setting up DHCP for ethernet")
    Popen(["ifconfig eth0 up"], shell=True)
    Popen(["pump -i eth0 --no-ntp -h DroneBridgeGND"], shell=True, close_fds=True, stdout=DEVNULL)
    Popen(["ifconfig eth0 up"], shell=True)


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
