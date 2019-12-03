#
# This file is part of DroneBridgeLib: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2019 Wolfgang Christl
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

import argparse
import signal
from syslog import LOG_ERR, LOG_INFO

from DroneBridge import DroneBridge, DBPort, DBDir, DBMode

from db_comm_messages import parse_comm_message, new_error_response_message, process_db_comm_protocol
from db_helpers import db_log, str2bool

keep_running = True


def parse_arguments():
    parser = argparse.ArgumentParser(description='Put this file on the UAV. It handles settings changes')
    parser.add_argument('-n', action='append', dest='DB_INTERFACES',
                        help='Network interfaces on which we send out packets to ground station. Should be interface '
                             'for long range comm', required=True)
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-a', action='store', type=str2bool, help='Use DB raw protocol compatibility mode',
                        dest='comp_mode', default=False)
    parser.add_argument('-c', action='store', type=int, dest='comm_id', required=True,
                        help='Communication ID must be the same on drone and groundstation. A number between 0-255 '
                             'Example: "125"', default='111')
    parser.add_argument('-f', action='store', type=int, dest='frametype', required=False,
                        help='[1|2] DroneBridgeLib v2 raw protocol packet/frame type: 1=RTS, 2=DATA (CTS protection)',
                        default='2')
    return parser.parse_args()


def signal_handler(signal, frame):
    global keep_running
    keep_running = False


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    parsedArgs = parse_arguments()
    mode = parsedArgs.mode
    list_interfaces = parsedArgs.DB_INTERFACES
    comm_id = bytes([parsedArgs.comm_id])
    comp_mode = parsedArgs.comp_mode
    frame_type = int(parsedArgs.frametype)
    db_log("DB_COMM_AIR: Communication ID: " + str(int.from_bytes(comm_id, byteorder='little')) +
           " (" + str(comm_id.hex()) + ")")
    db = DroneBridge(DBDir.DB_TO_GND, list_interfaces, DBMode.MONITOR, comm_id, DBPort.DB_PORT_COMMUNICATION,
                     tag="DB_COMM_AIR", db_blocking_socket=True, frame_type=frame_type, compatibility_mode=comp_mode)
    first_run = True
    while keep_running:
        if first_run:
            db.clear_socket_buffers()
            first_run = False
        db_comm_message_bytes = db.receive_data()
        if db_comm_message_bytes:
            try:
                comm_json = parse_comm_message(db_comm_message_bytes)
                if comm_json is not None:
                    db_log("DB_COMM_AIR: Got: " + str(comm_json), LOG_INFO)
                    response_msg = process_db_comm_protocol(comm_json, DBDir.DB_TO_GND)
                    db_log("DB_COMM_AIR: Responding...", LOG_INFO)
                    db.sendto_ground_station(response_msg, DBPort.DB_PORT_COMMUNICATION)
                else:
                    db_log("DB_COMM_AIR: Got corrupt message", LOG_INFO)
                    error_resp = new_error_response_message('DB_COMM_AIR: Corrupt message', DBDir.DB_TO_GND.value, 0000)
                    db.sendto_ground_station(error_resp, DBPort.DB_PORT_COMMUNICATION)
            except (UnicodeDecodeError, ValueError):
                db_log("DB_COMM_AIR: Received message from ground station with error. Not an UTF error or ValueError",
                       ident=LOG_ERR)
    db.close_sockets()
    db_log("DB_COMM_AIR: Terminated")
