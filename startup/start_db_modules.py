import argparse
import pyric.pyw as pyw
from subprocess import Popen

from Chipset import is_atheros_card
from common_helpers import read_dronebridge_config, PI3_WIFI_NIC, HOTSPOT_NIC

COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'
GND_STRING_TAG = 'DroneBridge GND: '
UAV_STRING_TAG = 'DroneBridge UAV: '


def parse_arguments():
    parser = argparse.ArgumentParser(description='This script starts all DroneBridge modules. First setup the wifi '
                                                 'adapters.')
    parser.add_argument('-g', action='store_true', dest='gnd',
                        help='start modules running on the ground station - if not set we start modules for UAV')
    return parser.parse_args()


def start_gnd_modules():
    """
    Reads the settings from the config file. Performs some checks and starts the DroneBridge modules on the ground station.
    :return:
    """
    config = read_dronebridge_config()
    communication_id = config.getint(COMMON, 'communication_id')
    datarate = config.getint(GROUND, 'datarate')
    interface_selection = config.get(GROUND, 'interface_selection')
    interface_control = config.get(GROUND, 'interface_control')
    interface_tel = config.get(GROUND, 'interface_tel')
    interface_video = config.get(GROUND, 'interface_video')
    interface_comm = config.get(GROUND, 'interface_comm')
    interface_proxy = config.get(GROUND, 'interface_proxy')
    en_control = config.get(GROUND, 'en_control')
    en_video = config.get(GROUND, 'en_video')
    en_comm = config.get(GROUND, 'en_comm')
    en_plugin = config.get(GROUND, 'en_plugin')
    rc_proto = config.getint(GROUND, 'rc_proto')
    en_rc_overwrite = config.get(GROUND, 'en_rc_overwrite')
    port_smartphone_ltm = config.getint(GROUND, 'port_smartphone_ltm')
    comm_port_local = config.getint(GROUND, 'comm_port_local')
    proxy_port_local_remote = config.getint(GROUND, 'proxy_port_local_remote')
    joy_interface = config.getint(GROUND, 'joy_interface')
    fwd_stream = config.get(GROUND, 'fwd_stream')
    fwd_stream_port = config.getint(GROUND, 'fwd_stream_port')
    video_mem = config.getint(GROUND, 'video_mem')

    # ---------- pre-init ------------------------
    print(GND_STRING_TAG + "Communication ID: " + str(communication_id))
    print(GND_STRING_TAG + "Trying to start individual modules...")
    if interface_selection == 'auto':
        interface_control = get_interface()
        interface_tel = interface_control
        interface_video = interface_control
        interface_comm = interface_control
        interface_proxy = interface_control

    # ----------- start modules ------------------------
    if en_comm == 'Y':
        print(GND_STRING_TAG + " Starting communication module...")
        Popen(["python3 communication/db_comm_ground.py -n "+interface_comm+" -p 1604 -u "+str(comm_port_local)
               + " -m m -c "+str(communication_id)+" &"], shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)

    print(GND_STRING_TAG + " Starting status module...")
    Popen(["./status/status -n "+interface_tel+" -m m -c "+str(communication_id)+" &"],
          shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)

    print(GND_STRING_TAG + " Starting proxy & telemetry module...")
    Popen(["./proxy/proxy -n "+interface_proxy+" -m m -p "+str(proxy_port_local_remote)+" -c "+str(communication_id)
           + " -i "+interface_tel+" -l "+str(port_smartphone_ltm)+" &"], shell=True, stdin=None, stdout=None,
          stderr=None, close_fds=True)

    if en_control == 'Y':
        print(GND_STRING_TAG + " Starting control module...")
        Popen(["./control/control_ground -n "+interface_control+" -j "+str(joy_interface)+" -m m -v "+str(rc_proto)
               + " -o "+en_rc_overwrite+" -c "+str(communication_id)+" &"], shell=True, stdin=None, stdout=None,
              stderr=None, close_fds=True)

    if en_plugin == 'Y':
        print(GND_STRING_TAG + " Starting plugin module...")
        Popen(["python3 plugin/db_plugin.py -g &"], shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)


def start_uav_modules():
    """
    Reads the settings from the config file. Performs some checks and starts the DroneBridge modules on the UAV.
    :return:
    """
    config = read_dronebridge_config()
    communication_id = config.getint(COMMON, 'communication_id')
    cts_protection = config.get(COMMON, 'cts_protection')
    datarate = config.getint(UAV, 'datarate')
    interface_selection = config.get(UAV, 'interface_selection')
    interface_control = config.get(UAV, 'interface_control')
    interface_tel = config.get(UAV, 'interface_tel')
    interface_video = config.get(UAV, 'interface_video')
    interface_comm = config.get(UAV, 'interface_comm')
    en_control = config.get(GROUND, 'en_control')
    en_video = config.get(GROUND, 'en_video')
    en_comm = config.get(GROUND, 'en_comm')
    en_plugin = config.get(GROUND, 'en_plugin')
    en_tel = config.get(GROUND, 'en_tel')
    video_blocks = config.getint(UAV, 'video_blocks')
    video_fecs = config.getint(UAV, 'video_fecs')
    video_blocklength = config.getint(UAV, 'video_blocklength')
    extraparams = config.get(UAV, 'extraparams')
    keyframerate = config.getint(UAV, 'keyframerate')
    width = config.getint(UAV, 'width')
    heigth = config.getint(UAV, 'heigth')
    video_bitrate = config.get(UAV, 'video_bitrate')
    video_channel_util = config.getint(UAV, 'video_channel_util')
    serial_int_tel = config.get(UAV, 'serial_int_tel')
    tel_proto = config.get(UAV, 'tel_proto')
    baud_tel = config.getint(UAV, 'baud_tel')
    serial_int_cont = config.get(UAV, 'serial_int_cont')
    baud_control = config.getint(UAV, 'baud_control')
    serial_prot = config.getint(UAV, 'serial_prot')
    pass_through_packet_size = config.getint(UAV, 'pass_through_packet_size')
    enable_sumd_rc = config.get(UAV, 'enable_sumd_rc')
    serial_int_sumd = config.get(UAV, 'serial_int_sumd')
    chipset_type_cont = 1

    # ---------- pre-init ------------------------
    if is_atheros_card(interface_control):
        chipset_type_cont = 2
    if interface_selection == 'auto':
        interface_control = get_interface()
        interface_tel = interface_control
        interface_video = interface_control
        interface_comm = interface_control

    # ---------- Error pre-check ------------------------
    if serial_int_cont == serial_int_tel and en_control == en_tel and en_control == 'Y':
        print(UAV_STRING_TAG + "Error - Control module and telemetry module are assigned to the same serial port. "
                               "Disabling telemetry module. Control module only supports MAVLink telemetry.")
        en_tel = 'N'
    if serial_int_cont == serial_int_sumd and en_control == 'Y' and enable_sumd_rc == 'Y':
        print(UAV_STRING_TAG + "Error - Control module and SUMD output are assigned to the same serial port. Disabling "
                               "SUMD.")
        enable_sumd_rc = 'N'
    print(UAV_STRING_TAG + "communication ID: " + str(communication_id))
    print(UAV_STRING_TAG + "Trying to start individual modules...")

    # ----------- start modules ------------------------
    if en_comm == 'Y':
        print(UAV_STRING_TAG + " Starting communication module...")
        Popen(["python3 communication/db_comm_air.py -n " + interface_comm + " -m m -c " + str(communication_id) +" &"],
              shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)

    if en_tel == 'Y':
        print(UAV_STRING_TAG + " Starting telemetry module...")
        Popen(["./telemetry/telemetry_air -n "+interface_tel+" -f "+serial_int_tel+" -r "+str(baud_tel)+" -m m -c " +
               str(communication_id) + " -l "+str(tel_proto)+" &"], shell=True, stdin=None, stdout=None, stderr=None,
              close_fds=True)

    if en_control == 'Y':
        print(UAV_STRING_TAG + " Starting control module...")
        Popen(["./control/control_air -n " + interface_control + " -u " + serial_int_cont + " -m m -c "
               + str(communication_id) + " -a " + str(chipset_type_cont) + " -v " + str(serial_prot) + " -l "
               + str(pass_through_packet_size) + " -r " + str(baud_control) + " -e " + enable_sumd_rc + " -s "
               + serial_int_sumd + " &"],
              shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)

    if en_plugin == 'Y':
        print(UAV_STRING_TAG + " Starting plugin module...")
        Popen(["python3 plugin/db_plugin.py &"], shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)


def get_interface():
    """
    Find a possibly working wifi interface that can be used by a DroneBridge module
    :return: Name of an interface set to monitor mode
    """
    interface_names = pyw.winterfaces()
    for interface_name in interface_names:
        if interface_name != PI3_WIFI_NIC and interface_name != HOTSPOT_NIC:
            card = pyw.getcard(interface_name)
            if pyw.modeget(card) == 'monitor':
                return interface_name


def main():
    parsed_args = parse_arguments()
    if parsed_args.gnd:
        start_gnd_modules()
    else:
        start_uav_modules()


if __name__ == "__main__":
    main()

