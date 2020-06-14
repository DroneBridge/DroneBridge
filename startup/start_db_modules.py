import argparse
import os
import pyric.pyw as pyw
import pyric.utils.hardware as iwhw
import subprocess
import time

from DroneBridge import DroneBridge, DBDir, DBMode, DBPort, db_log

from Chipset import is_atheros_card, is_realtek_card, is_ralink_card
from common_helpers import read_dronebridge_config, PI3_WIFI_NIC, HOTSPOT_NIC, get_bit_rate
from socket import *
from subprocess import Popen

COMMON = 'COMMON'
GROUND = 'GROUND'
UAV = 'AIR'
GND_STRING_TAG = 'DroneBridge GND: '
UAV_STRING_TAG = 'DroneBridge UAV: '
DRONEBRIDGE_BIN_PATH = os.path.join(os.sep, "home", "pi", "DroneBridge")


def parse_arguments():
    parser = argparse.ArgumentParser(description='This script starts all DroneBridge modules. First setup the wifi '
                                                 'adapters.')
    parser.add_argument('-g', action='store_true', dest='gnd', default=False,
                        help='start modules running on the ground station - if not set we start modules for UAV')
    return parser.parse_args()


def start_gnd_modules():
    """
    Reads the settings from the config file. Performs some checks and starts the DroneBridge modules on the ground station.
    """
    config = read_dronebridge_config()
    if config is None:
        exit(-1)
    communication_id = config.getint(COMMON, 'communication_id')
    cts_protection = config.get(COMMON, 'cts_protection')
    fps = config.getfloat(COMMON, 'fps')
    video_blocks = config.getint(COMMON, 'video_blocks')
    video_fecs = config.getint(COMMON, 'video_fecs')
    video_blocklength = config.getint(COMMON, 'video_blocklength')
    compatibility_mode = config.getint(COMMON, 'compatibility_mode')
    datarate = config.getint(GROUND, 'datarate')
    interface_selection = config.get(GROUND, 'interface_selection')
    interface_control = config.get(GROUND, 'interface_control')
    interface_video = config.get(GROUND, 'interface_video')
    interface_comm = config.get(GROUND, 'interface_comm')
    interface_proxy = config.get(GROUND, 'interface_proxy')
    en_control = config.get(GROUND, 'en_control')
    en_video = config.get(GROUND, 'en_video')
    en_comm = config.get(GROUND, 'en_comm')
    en_plugin = config.get(GROUND, 'en_plugin')
    rc_proto = config.getint(GROUND, 'rc_proto')
    en_rc_overwrite = config.get(GROUND, 'en_rc_overwrite')
    joy_interface = config.getint(GROUND, 'joy_interface')
    fwd_stream = config.get(GROUND, 'fwd_stream')
    fwd_stream_port = config.getint(GROUND, 'fwd_stream_port')

    # ---------- pre-init ------------------------
    print(GND_STRING_TAG + "Communication ID: " + str(communication_id))
    print(GND_STRING_TAG + "Trying to start individual modules...")
    if interface_selection == 'auto':
        interface_control = get_all_monitor_interfaces(True)
        print(f"\tUsing: {interface_control} for all modules")
        interface_video = interface_control
        interface_comm = interface_control
        interface_proxy = interface_control
    frametype = determine_frametype(cts_protection, get_interface())  # TODO: scan for WiFi traffic on all interfaces

    # ----------- start modules ------------------------
    if en_comm == 'Y':
        print(f"{GND_STRING_TAG} Starting communication module...")
        comm = ["python3", os.path.join(DRONEBRIDGE_BIN_PATH, 'communication', 'db_communication_gnd.py'), "-m", "m",
                "-c", str(communication_id), "-a", str(compatibility_mode), "-f", str(frametype)]
        comm.extend(interface_comm.split())
        Popen(comm, shell=False, stdin=None, stdout=None, stderr=None)

    print(f"{GND_STRING_TAG} Starting status module...")
    comm_status = [os.path.join(DRONEBRIDGE_BIN_PATH, 'status', 'db_status'), "-m", "m", "-c", str(communication_id)]
    comm_status.extend(interface_proxy.split())
    Popen(comm_status, shell=False, stdin=None, stdout=None, stderr=None)

    print(f"{GND_STRING_TAG} Starting proxy module...")
    comm_proxy = [os.path.join(DRONEBRIDGE_BIN_PATH, 'proxy', 'db_proxy'), "-m", "m", "-c", str(communication_id),
                  "-f", str(frametype), "-b", str(get_bit_rate(datarate)), "-a", str(compatibility_mode)]
    comm_proxy.extend(interface_proxy.split())
    Popen(comm_proxy, shell=False, stdin=None, stdout=None, stderr=None)

    if en_control == 'Y':
        print(f"{GND_STRING_TAG} Starting control module...")
        comm_control = [os.path.join(DRONEBRIDGE_BIN_PATH, 'control', 'control_ground'), "-j",
                        str(joy_interface), "-m", "m", "-v", str(rc_proto), "-o", str(en_rc_overwrite), "-c",
                        str(communication_id), "-t", str(frametype), "-b", str(get_bit_rate(datarate)), "-a",
                        str(compatibility_mode)]
        comm_control.extend(interface_control.split())
        Popen(comm_control, shell=False, stdin=None, stdout=None, stderr=None, close_fds=True)

    if en_plugin == 'Y':
        print(GND_STRING_TAG + "Starting plugin module...")
        Popen(["python3", os.path.join(DRONEBRIDGE_BIN_PATH, 'plugin', 'db_plugin.py'), "-g"],
              shell=False, stdin=None, stdout=None, stderr=None, close_fds=True)

    print(GND_STRING_TAG + "Starting USBBridge module...")
    Popen([os.path.join(DRONEBRIDGE_BIN_PATH, 'usbbridge', 'usbbridge'), "-v", en_video, "-c", en_comm, "-p", "Y",
           "-s", "Y"], shell=False, stdin=None, stdout=None, stderr=None, close_fds=True)

    if en_video == 'Y':
        print(f"{GND_STRING_TAG} Starting video module... (FEC: {video_blocks}/{video_fecs}/{video_blocklength})")
        receive_comm = [os.path.join(DRONEBRIDGE_BIN_PATH, 'video', 'video_gnd'), "-d", str(video_blocks),
                        "-r", str(video_fecs), "-f", str(video_blocklength), "-c", str(communication_id), "-p", "N",
                        "-v", str(fwd_stream_port), "-o"]
        receive_comm.extend(interface_video.split())
        db_video_receive = Popen(receive_comm, stdout=subprocess.PIPE, close_fds=True, shell=False, bufsize=0)
        print(f"{GND_STRING_TAG} Starting video player...")
        Popen([get_video_player(fps)], stdin=db_video_receive.stdout, close_fds=True, shell=False)


def start_uav_modules():
    """
    Reads the settings from the config file. Performs some checks and starts the DroneBridge modules on the UAV.
    """
    config = read_dronebridge_config()
    if config is None:
        exit(-1)
    communication_id = config.getint(COMMON, 'communication_id')
    cts_protection = config.get(COMMON, 'cts_protection')
    compatibility_mode = config.getint(COMMON, 'compatibility_mode')
    datarate = config.getint(UAV, 'datarate')
    interface_selection = config.get(UAV, 'interface_selection')
    interface_control = config.get(UAV, 'interface_control')
    interface_video = config.get(UAV, 'interface_video')
    interface_comm = config.get(UAV, 'interface_comm')
    en_control = config.get(UAV, 'en_control')
    en_video = config.get(UAV, 'en_video')
    en_comm = config.get(UAV, 'en_comm')
    en_plugin = config.get(UAV, 'en_plugin')
    video_blocks = config.getint(COMMON, 'video_blocks')
    video_fecs = config.getint(COMMON, 'video_fecs')
    video_blocklength = config.getint(COMMON, 'video_blocklength')
    extraparams = config.get(UAV, 'extraparams')
    keyframerate = config.getint(UAV, 'keyframerate')
    width = config.getint(UAV, 'width')
    height = config.getint(UAV, 'height')
    fps = config.getfloat(COMMON, 'fps')
    video_bitrate = config.get(UAV, 'video_bitrate')
    video_channel_util = config.getint(UAV, 'video_channel_util')
    en_video_rec = config.get(UAV, 'en_video_rec')
    video_mem = config.get(UAV, 'video_mem')    # path to video files
    serial_int_cont = config.get(UAV, 'serial_int_cont')
    baud_control = config.getint(UAV, 'baud_control')
    serial_prot = config.getint(UAV, 'serial_prot')
    pass_through_packet_size = config.getint(UAV, 'pass_through_packet_size')
    enable_sumd_rc = config.get(UAV, 'enable_sumd_rc')
    serial_int_sumd = config.get(UAV, 'serial_int_sumd')

    # ---------- pre-init ------------------------
    if interface_selection == 'auto':
        interface_control = get_all_monitor_interfaces(True)
        print(f"\tUsing: {interface_control} for all modules")
        interface_video = interface_control
        interface_comm = interface_control
    frametype = determine_frametype(cts_protection, get_interface())  # TODO: scan for WiFi traffic on all interfaces
    if video_bitrate == 'auto' and en_video == 'Y':
        video_bitrate = int(measure_available_bandwidth(video_blocks, video_fecs, video_blocklength, frametype,
                                                        datarate, get_all_monitor_interfaces(False)))
        print(f"{UAV_STRING_TAG} Available bandwidth is {video_bitrate / 1000} kbit/s")
        video_bitrate = int(video_channel_util / 100 * int(video_bitrate))
        print(
            f"{UAV_STRING_TAG} Setting video bitrate to {video_bitrate / 1000} kbit/s")
    else:
        video_bitrate = str(int(video_bitrate) * 1000)  # convert to bit/s (user enters kbit)

    # ---------- Error pre-check ------------------------
    if serial_int_cont == serial_int_sumd and en_control == 'Y' and enable_sumd_rc == 'Y':
        print(UAV_STRING_TAG + "Error - Control module and SUMD output are assigned to the same serial port. Disabling "
                               "SUMD")
        enable_sumd_rc = 'N'
    print(f"{UAV_STRING_TAG} Communication ID: {communication_id}")
    print(f"{UAV_STRING_TAG} Trying to start individual modules...")

    # ----------- start modules ------------------------
    if en_comm == 'Y':
        print(f"{UAV_STRING_TAG} Starting communication module...")
        comm = ["python3", os.path.join(DRONEBRIDGE_BIN_PATH, 'communication', 'db_communication_air.py'), "-m", "m",
                "-c", str(communication_id), "-a", str(compatibility_mode), "-f", str(frametype)]
        comm.extend(interface_comm.split())
        Popen(comm, shell=False, stdin=None, stdout=None, stderr=None)

    if en_control == 'Y':
        print(f"{UAV_STRING_TAG} Starting control module...")
        comm = [os.path.join(DRONEBRIDGE_BIN_PATH, 'control', 'control_air'), "-u", str(serial_int_cont), "-m", "m",
                "-c", str(communication_id), "-v", str(serial_prot), "-t", str(frametype), "-l",
                str(pass_through_packet_size), "-r", str(baud_control), "-e", str(enable_sumd_rc), "-s",
                str(serial_int_sumd), "-b", str(get_bit_rate(2))]
        comm.extend(interface_control.split())
        Popen(comm, shell=False, stdin=None, stdout=None, stderr=None)

    if en_plugin == 'Y':
        print(f"{UAV_STRING_TAG} Starting plugin module...")
        Popen(["python3", os.path.join(DRONEBRIDGE_BIN_PATH, 'plugin', 'db_plugin.py')],
              shell=False, stdin=None, stdout=None, stderr=None)

    if en_video == 'Y':
        print(f"{UAV_STRING_TAG} Starting video transmission, FEC {video_blocks}/{video_fecs}/{video_blocklength} : "
              f"{width} x {height} fps {fps}, video bitrate: {video_bitrate} bit/s, key framerate: {keyframerate} "
              f"frame type: {frametype}")

        raspivid_comm = ["raspivid", "-w", str(width), "-h", str(height), "-fps", str(fps), "-b", str(video_bitrate),
                         "-g", str(keyframerate), "-t", "0"]
        raspivid_comm.extend(extraparams.split())
        raspivid_comm.extend(["-o", "-"])
        raspivid_task = Popen(raspivid_comm, stdout=subprocess.PIPE, stdin=None, stderr=None, close_fds=True,
                              shell=False, bufsize=0)

        video_air_comm = [os.path.join(DRONEBRIDGE_BIN_PATH, 'video', 'video_air'), "-d", str(video_blocks), "-r",
                          str(video_fecs), "-f", str(video_blocklength), "-t", str(frametype),
                          "-b", str(get_bit_rate(datarate)), "-c", str(communication_id), "-a", str(compatibility_mode)]
        video_air_comm.extend(interface_video.split())
        Popen(video_air_comm, stdin=raspivid_task.stdout, stdout=None, stderr=None, close_fds=True, shell=False)

        if en_video_rec == 'Y' and video_mem != "":
            if video_mem.endswith("/"):
                video_mem = video_mem[:-1]
            print(f"{UAV_STRING_TAG} Starting video recording to {video_mem}")
            comm = [os.path.join(DRONEBRIDGE_BIN_PATH, 'recorder', 'db_recorder'), video_mem]
            Popen(comm, shell=False, stdin=None, stdout=None, stderr=None)


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
    print("ERROR: Could not find a wifi adapter in monitor mode")
    exit(-1)


def get_interfaces() -> list:
    """
    Find a possibly working wifi interfaces that can be used by a DroneBridge modules

    :return: List of names of interfaces set to monitor mode
    """
    interfaces = []
    interface_names = pyw.winterfaces()
    for interface_name in interface_names:
        if interface_name != PI3_WIFI_NIC and interface_name != HOTSPOT_NIC:
            card = pyw.getcard(interface_name)
            if pyw.modeget(card) == 'monitor':
                interfaces.append(interface_name)
    if len(interfaces) == 0:
        print("ERROR: Could not find a wifi adapter in monitor mode")
        exit(-1)
    else:
        return interfaces


def get_all_monitor_interfaces(formatted=False):
    """
    Find all possibly working wifi interfaces that can be used by a DroneBridge modules

    :param formatted: Formatted to be used as input for DroneBridge modules
    :return: List of names of interfaces set to monitor mode
    """
    w_interfaces = []
    interface_names = pyw.winterfaces()
    for interface_name in interface_names:
        if interface_name != PI3_WIFI_NIC and interface_name != HOTSPOT_NIC:
            card = pyw.getcard(interface_name)
            if pyw.modeget(card) == 'monitor':
                w_interfaces.append(interface_name)
    if formatted:
        formated_str = ""
        for w_int in w_interfaces:
            formated_str = formated_str + " -n " + w_int
        return formated_str[1:]
    else:
        formated_str = ""
        for w_int in w_interfaces:
            formated_str = formated_str + " " + w_int
        return formated_str[1:]


def measure_available_bandwidth(video_data_packets, video_fecs_packets, packet_size, video_frametype, datarate,
                                interface_video, sleep_time=0.025) -> float:
    """
    Measure available network capacity. This function respects the FEC overhead. The returned value is bandwidth - FEC.
    In other words: How much payload data can I send & protect with FEC within the given environment

    This does not reflect the real free channel capacity. Measurement error too high. Keep it because we do not have
    anything better.

    :param video_data_packets:  Data packets per block
    :param video_fecs_packets:  FEC packets per block
    :param packet_size:   Packet size
    :param video_frametype:
    :param datarate:
    :param interface_video:
    :param sleep_time: Time to sleep between injection of packets to get more realistic results
    :return: Capacity in bit per second
    """
    db_log("Measuring available bitrate")
    packet_real_size = 4 + packet_size  # FEC header + data
    dummy_data = bytes([1] * packet_real_size)
    sent_data_bytes = 0
    measure_time = 3.5  # seconds
    time.sleep(1)
    dronebridge = DroneBridge(DBDir.DB_TO_GND, [interface_video], DBMode.MONITOR, 15, DBPort.DB_PORT_VIDEO,
                              tag="BitrateMeasure", db_blocking_socket=True, frame_type=video_frametype,
                              transmission_bitrate=datarate)
    dronebridge.sendto_ground_station(dummy_data, DBPort.DB_PORT_VIDEO)  # first measurement may be flawed
    start = time.time()
    while (time.time() - start) < measure_time:
        dronebridge.sendto_ground_station(dummy_data, DBPort.DB_PORT_VIDEO)
        sent_data_bytes += packet_real_size
        time.sleep(sleep_time)
    return ((sent_data_bytes * (video_data_packets / (video_data_packets + video_fecs_packets))) * 8) / measure_time


def get_video_player(fps):
    """
    mmormota's stutter-free implementation based on RiPis hello_video.bin: "hello_video.bin.30" (for 30fps) or
    "hello_video.bin.48" (for 48 and 59.9fps)
    befinitiv's hello_video.bin: "hello_video.bin.240" (for any fps, use this for higher than 59.9fps)

    :param fps: The video fps that is set in the config
    :return: The path to the video player binary
    """

    if fps == 30:
        return os.path.join(DRONEBRIDGE_BIN_PATH, 'video', 'pi_video_player', 'db_pi_player_30')
    elif fps <= 60:
        return os.path.join(DRONEBRIDGE_BIN_PATH, 'video', 'pi_video_player', 'db_pi_player_48')
    else:
        return os.path.join(DRONEBRIDGE_BIN_PATH, 'video', 'pi_video_player', 'db_pi_player_240')


def exists_wifi_traffic(wifi_interface):
    """
    Checks for wifi traffic on a monitor interface

    :param wifi_interface: The interface to listen for traffic
    :return: True if there is wifi traffic
    """
    raw_socket = socket(AF_PACKET, SOCK_RAW, htons(0x0004))
    raw_socket.bind((wifi_interface, 0))
    raw_socket.settimeout(2)  # wait x seconds for traffic
    try:
        received_data = raw_socket.recv(2048)
        if len(received_data) > 0:
            db_log("Detected WiFi traffic on channel")
            raw_socket.close()
            return True
    except timeout:
        raw_socket.close()
        return False
    raw_socket.close()
    return False


def determine_frametype(cts_protection, interface_name):
    """
    Checks if there is wifi traffic. And determines the to use frame type (DATA, RTS, BEACON)

    :param cts_protection: The value from the DroneBridgeConfig
    :param interface_name: The interface to listen for traffic
    :return

    - 1 for RTS frames
    - 2 for data frames
    - 3 for beacon frames (reception not supported)
    """
    print("Determining frame type...")
    wifi_driver = iwhw.ifdriver(interface_name)
    if is_ralink_card(wifi_driver):
        return 2  # injection with rt2800usb and RTS broken?!
    elif is_realtek_card(wifi_driver):
        return 2  # use data frames (~1Mbps with rtl8814au an RTS)
    elif cts_protection == 'Y' and is_atheros_card(wifi_driver):
        return 2  # standard data frames
    elif cts_protection == 'auto':
        if is_atheros_card(wifi_driver) and exists_wifi_traffic(interface_name):
            return 2  # standard data frames
        else:
            return 1  # RTS frames
    else:
        return 1  # RTS frames


if __name__ == "__main__":
    print("---- Starting modules")
    parsed_args = parse_arguments()
    if parsed_args.gnd:
        start_gnd_modules()
    else:
        start_uav_modules()
    print("---- Done with startup")
