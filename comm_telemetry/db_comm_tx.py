import argparse
from DroneBridge_Protocol import DBProtocol
from db_comm_helper import find_mac
import time

# Default values, may get overridden by command line arguments
UDP_Port_RX = 1604  # Port for communication with RX (Drone)
IP_RX = '192.168.3.1'  # Target IP address (IP address of the Pi on the Drone: needs fixed one)
UDP_PORT_ANDROID = 1605  # Port for communication with smartphone (port on groundstation side)
UDP_buffersize = 512  # bytes
interface_drone_comm = "000ee8dcaa2c" # for testing
dst = b''   # MAC address of RX-Pi (TP-Link) - mac of drone
# TODO: at the moment comm_id must be same as dest for compatibility reasons of v1 and v2 of raw protocol. Otherwise no MSP command can be sent


def parsearguments():
    parser = argparse.ArgumentParser(description='Put this file on TX (drone). It handles telemetry, GoPro settings'
                                                 ' and communication with smartphone')
    parser.add_argument('-i', action='store', dest='interface_drone_comm',
                        help='Network interface on which we send out packets to MSP-pass through. Should be interface '
                        'for long range comm (default: wlan1)',
                        default='wlan1')
    parser.add_argument('-p', action="store", dest='udp_port_rx',
                        help='Local and remote port on which we need to address '
                             'our packets for the drone and listen for '
                             'commands coming from drone (same port '
                             'number on TX and RX - you may not change'
                             ' default: 1604)', type=int, default=1604)
    parser.add_argument('-r', action='store', dest='ip_rx', help='IP address of RX (drone) (default: 192.168.3.1)',
                        default='192.168.3.1')
    parser.add_argument('-u', action='store', dest='udp_port_android',
                        help='Port we listen on for incoming packets from '
                             'smartphone (default: 1605)',
                        default=1605, type=int)
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-a', action='store', dest='frame_type',
                        help='Specify frame type. Use <1> for Ralink chips (data frame) and <2> for Atheros chips '
                             '(beacon frame). No CTS supported. Options [1|2]', default='1')
    parser.add_argument('-c', action='store', dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. 8 characters long. Allowed '
                             'chars are (0123456789abcdef) Example: "aabb0011"', default='aabbccdd')
    return parser.parse_args()


def main():
    global interface_drone_comm, IP_RX, UDP_Port_RX, UDP_PORT_ANDROID
    parsedArgs = parsearguments()
    interface_drone_comm = parsedArgs.interface_drone_comm
    mode = parsedArgs.mode
    IP_RX = parsedArgs.ip_rx
    UDP_Port_RX = parsedArgs.udp_port_rx
    UDP_PORT_ANDROID = parsedArgs.udp_port_android
    frame_type = parsedArgs.frame_type

    src = find_mac(interface_drone_comm)
    extended_comm_id = bytes(b'\x01'+b'\x01'+bytearray.fromhex(parsedArgs.comm_id)) # <odd><direction><comm_id>
    # print("DB_TX_Comm: Communication ID: " + comm_id.hex()) # only works in python 3.5+
    print("DB_TX_Comm: Communication ID: " + str(extended_comm_id))

    dbprotocol = DBProtocol(src, dst, UDP_Port_RX, IP_RX, UDP_PORT_ANDROID, b'\x01', interface_drone_comm, mode,
                            extended_comm_id, frame_type, b'\x04')
    if mode == 'wifi':
        dbprotocol.updateRouting()

    last_keepalive = 0

    while True:
        last_keepalive = dbprotocol.process_smartphonerequests(last_keepalive)  # non-blocking
        time.sleep(0.5)

if __name__ == "__main__":
    main()
