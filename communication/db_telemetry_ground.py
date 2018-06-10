# This file is part of DroneBridge licenced under Apache Licence 2
# https://github.com/seeul8er/DroneBridge/
# Created by Wolfgang Christl

import argparse

from DroneBridge_Protocol import DBProtocol, DBPort, DBDir
from db_comm_helper import find_mac

UDP_Port_RX = 1604  # Port for communication with RX (Drone)
IP_RX = '192.168.3.1'  # Target IP address (IP address of the Pi on the Drone: needs fixed one)
UDP_PORT_ANDROID = 1605  # Port for communication with smartphone (port on groundstation side)
UDP_buffersize = 512  # bytes
interface_drone_comm = "000ee8dcaa2c"
pipenames = ["telemetryfifo1", "telemetryfifo2", "telemetryfifo3", "telemetryfifo4", "telemetryfifo5", "telemetryfifo6"]
pipes = []
fifo_write = None


def write_tofifos(received_bytes):
    try:
        fifo_write.write(received_bytes)
        fifo_write.flush()
        return True
    except BrokenPipeError as bperr:
        # print("DB_TX_TEL: Broken pipe: "+str(bperr.strerror))
        return False
    except OSError as oserr:
        print("DB_TEL_GROUND: Pipe might not be opened yet: "+str(oserr.strerror))
        return False


def parsearguments():
    parser = argparse.ArgumentParser(description='Put this file on the groundstation. It handles telemetry, GoPro '
                                                 'settings and communication with smartphone')
    parser.add_argument('-n', action='store', dest='interface_drone_comm',
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
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-c', action='store', type=int, dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. A number between 0-255 '
                             'Example: "125"', default='111')
    return parser.parse_args()


def main():
    global interface_drone_comm, IP_RX, UDP_Port_RX, fifo_write
    parsedArgs = parsearguments()
    interface_drone_comm = parsedArgs.interface_drone_comm
    mode = parsedArgs.mode
    IP_RX = parsedArgs.ip_rx
    UDP_Port_RX = parsedArgs.udp_port_rx

    comm_id = bytes([parsedArgs.comm_id])
    print("DB_TEL_GROUND: Communication ID: " + str(comm_id))

    dbprotocol = DBProtocol(UDP_Port_RX, IP_RX, 1606, DBDir.DB_TO_UAV, interface_drone_comm, mode,
                            comm_id, DBPort.DB_PORT_TELEMETRY, tag='DB_TEL: ')
    print("DB_TEL_GROUND: Opening /root/telemetryfifo1...")
    fifo_write = open("/root/telemetryfifo1", "wb")
    print("DB_TEL_GROUND: Opened /root/telemetryfifo1")

    while True:
        received = dbprotocol.receive_telemetryfromdrone()
        if received != False:
            try:
                write_tofifos(received)
                sent = dbprotocol.sendto_smartphone(received, dbprotocol.APP_PORT_TEL)
                # print("DB_TX_TEL: Sent "+str(sent)+" bytes to sp")
            except Exception as e:
                print(e)


if __name__ == "__main__":
    main()
