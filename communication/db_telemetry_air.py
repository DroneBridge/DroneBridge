#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2017 Wolfgang Christl
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

import socket
import serial
import argparse
from subprocess import Popen
from DroneBridge_Protocol import DBProtocol, DBPort, DBDir
from db_comm_helper import find_mac
import time


UDP_Port_TX = 1604  # Port for communication with TX (Groundstation)
IP_TX = "192.168.3.2"   # Target IP address (IP address of the Groundstation - not important and gets overridden anyways)
UDP_buffersize = 512  # bytes
SerialPort = '/dev/ttyAMA0'  # connect this one to your flight controller

# LTM: payload+crc
LTM_sizeGPS = 15
LTM_sizeAtt = 7
LTM_sizeStatus = 8

MavLink_chunksize = 128  # bytes


def openTXUDP_Socket():
    print("DB_TEL_AIR: Opening UDP socket towards TX-Pi - listening on port " + str(UDP_Port_TX))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_address = ('', UDP_Port_TX)
    sock.bind(server_address)
    return sock


def openFCTel_Socket(baud_rate):
    print("DB_TEL_AIR: Opening telemetry socket " + SerialPort + " (to listen to FC)")
    ser = serial.Serial(SerialPort, baudrate=baud_rate, timeout=None)
    return ser


def getGoPro_Status_JSON():
    return b"GoPro-Test Status"
    #return requests.get('http://10.5.5.9/gp/gpControl/status').json()


def read_LTM_Frame(functionbyte, serial_socket):
    """:returns complete LTM frame"""
    if functionbyte == b'A':
        return bytes(bytearray(b'$TA' + serial_socket.read(LTM_sizeAtt)))
    elif functionbyte == b'S':
        return bytes(bytearray(b'$TS' + serial_socket.read(LTM_sizeStatus)))
    elif functionbyte == b'G':
        return bytes(bytearray(b'$TG' + serial_socket.read(LTM_sizeGPS)))
    elif functionbyte == b'O':
        return bytes(bytearray(b'$TO' + serial_socket.read(LTM_sizeGPS)))
    elif functionbyte == b'N':
        return bytes(bytearray(b'$TN' + serial_socket.read(LTM_sizeAtt)))
    elif functionbyte == b'X':
        return bytes(bytearray(b'$TX' + serial_socket.read(LTM_sizeAtt)))
    else:
        print("DB_TEL_AIR: unknown frame!")
        return b'$T?'


def isitLTM_telemetry(telemetry_socket):
    for i in range(1, 50):
        if telemetry_socket.read() == b'$':
            if telemetry_socket.read() == b'T':
                if check_LTM_crc_valid(read_LTM_Frame(telemetry_socket.read(), telemetry_socket)):
                    print("DB_TEL_AIR: Detected LTM telemetry stream")
                    return True
    print("DB_TEL_AIR: Detected possible MavLink telemetry stream.")
    return False


def check_LTM_crc_valid(bytes_LTM_complete):
    """bytes_LTM_payload_crc is LTM payload+crc bytes"""
    crc = 0x00
    for byte in bytes_LTM_complete[3:]:
        crc = crc ^ (byte & 0xFF)
    return crc == 0


def setupVideo(mode):
    if mode == "wifi":
        proc = Popen("pp_rx_keepgopro.py", shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)


def parseArguments():
    parser = argparse.ArgumentParser(description='Put this file on your drone. It handles telemetry.')
    parser.add_argument('-n', action='store', dest='DB_INTERFACE',
                        help='Network interface on which we send out packets to drone. Should be interface '
                             'for long range comm (default: wlan1)',
                        default='wlan1')
    parser.add_argument('-f', action='store', dest='serialport', help='Serial port which is connected to flight controller'
                                                                      ' and receives the telemetry (default: /dev/ttyAMA0)',
                        default='/dev/ttyAMA0')
    parser.add_argument('-l', action='store', dest='telemetry_type',
                        help='Set telemetry type manually. Default is [auto]. Use [ltm|mavlink|auto]',
                        default='auto')
    parser.add_argument('-p', action="store", dest='udp_port_tx', help='Local and remote port on which we need to address '
                                                                       'our packets for the groundstation and listen for '
                                                                       'commands coming from groundstation (same port '
                                                                       'number on TX and RX - you may not change'
                                                                       ' default: 1604)', type=int, default=1604)
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-r', action='store', type=int, dest='baudrate',
                        help='Baudrate for the serial port: [115200|57600|38400|19200|9600|4800|2400]', default='9600')
    parser.add_argument('-c', action='store', type=int, dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. A number between 0-255 '
                             'Example: "125"', default='111')
    return parser.parse_args()


def main():
    global SerialPort, UDP_Port_TX, IP_TX
    parsedArgs = parseArguments()
    SerialPort = parsedArgs.serialport
    UDP_Port_TX = parsedArgs.udp_port_tx
    mode = parsedArgs.mode
    DB_INTERFACE = parsedArgs.DB_INTERFACE
    telemetry_selection_auto = False
    isLTMTel = True
    if parsedArgs.telemetry_type == "mavlink":
        isLTMTel = False
    elif parsedArgs.telemetry_type == "auto":
        telemetry_selection_auto = True
        isLTMTel = False
    comm_id = bytes([parsedArgs.comm_id])
    # print("DB_TEL_AIR: Communication ID: " + comm_id.hex()) # only works in python 3.5
    print("DB_TEL_AIR: Communication ID: " + str(comm_id))
    dbprotocol = DBProtocol(UDP_Port_TX, IP_TX, 0, DBDir.DB_TO_GND, DB_INTERFACE, mode, comm_id,
                            DBPort.DB_PORT_TELEMETRY, tag='DB_TEL_AIR: ')

    tel_sock = openFCTel_Socket(parsedArgs.baudrate)
    time.sleep(0.3)
    if telemetry_selection_auto:
        isLTMTel = isitLTM_telemetry(tel_sock)
    time.sleep(0.3)

    while True:
        if isLTMTel:
            if tel_sock.read() == b'$':
                tel_sock.read()  # next one is always a 'T' (do not care)
                LTM_Frame = read_LTM_Frame(tel_sock.read(), tel_sock)
                dbprotocol.sendto_groundstation(LTM_Frame, b'\x02')
        else:
            # it is not LTM --> fully transparent link for MavLink and other protocols
            dbprotocol.sendto_groundstation(tel_sock.read(MavLink_chunksize), b'\x02')


if __name__ == "__main__":
    main()
