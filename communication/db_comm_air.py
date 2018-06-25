#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2018 Wolfgang Christl
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

import argparse
from subprocess import Popen
from DroneBridge_Protocol import DBProtocol, DBPort, DBDir
from db_comm_helper import find_mac


UDP_Port_TX = 1604  # Port for communication with TX (Groundstation)
IP_TX = "192.168.3.2"   # Target IP address (IP address of the Groundstation - not important and gets overridden anyways)
UDP_buffersize = 512  # bytes
AB_INTERFACE = "wlan1"
dst = b''

def getGoPro_Status_JSON():
    return b"GoPro-Test Status"
    #return requests.get('http://10.5.5.9/gp/gpControl/status').json()


def setupVideo(mode):
    if mode == "wifi":
        proc = Popen("pp_rx_keepgopro.py", shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)


def parseArguments():
    parser = argparse.ArgumentParser(description='Put this file on RX (drone). It handles telemetry and GoPro settings.')
    parser.add_argument('-n', action='store', dest='DB_INTERFACE',
                        help='Network interface on which we send out packets to drone. Should be interface '
                             'for long range comm (default: wlan1)',
                        default='wlan1')
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-c', action='store', type=int, dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. A number between 0-255 '
                             'Example: "125"', default='111')
    return parser.parse_args()


def main():
    global SerialPort, UDP_Port_TX, IP_TX
    parsedArgs = parseArguments()
    UDP_Port_TX = 1604
    mode = parsedArgs.mode
    DB_INTERFACE = parsedArgs.DB_INTERFACE
    comm_id = bytes([parsedArgs.comm_id])
    # print("DB_TX_Comm: Communication ID: " + comm_id.hex()) # only works in python 3.5+
    print("DB_Comm_Air: Communication ID: " + str(comm_id))
    dbprotocol = DBProtocol(UDP_Port_TX, IP_TX, 0, DBDir.DB_TO_GND, DB_INTERFACE, mode, comm_id,
                            DBPort.DB_PORT_COMMUNICATION, tag='DB_Comm_Air: ')

    while True:
        dbprotocol.receive_process_datafromgroundstation() # blocking


if __name__ == "__main__":
    main()
