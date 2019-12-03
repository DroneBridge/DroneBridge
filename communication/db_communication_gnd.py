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
#
import argparse
import signal
from select import select, error
from socket import socket, AF_INET, SOCK_STREAM, SO_REUSEADDR, SOL_SOCKET
from syslog import LOG_ERR, LOG_WARNING, LOG_DEBUG

from DroneBridge import DroneBridge, DBPort, DBDir, DBMode
from DroneBridge.db_helpers import str2bool, db_log

from DBCommProt import DBCommProt
from db_comm_messages import parse_comm_message, new_error_response_message, process_db_comm_protocol

keep_running = True


def parse_arguments():
    parser = argparse.ArgumentParser(description='Put this file on the ground station. The program handles settings '
                                                 'changes and routing to UAV')
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


def open_tcp_socket() -> socket:
    tcp_socket = socket(AF_INET, SOCK_STREAM)
    tcp_socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    tcp_socket.bind(('', 1603))
    # tcp_socket.setblocking(False)
    tcp_socket.listen(5)
    return tcp_socket


def signal_handler(signal, frame):
    global keep_running
    keep_running = False


def process_comm_proto(db_comm_message_bytes: bytes, _tcp_connections: list):
    """
    Process and route communication messages on the ground station side

    :param _tcp_connections: List of sockets of connected tcp clients
    :param db_comm_message_bytes: Raw byte representation of the message
    """
    try:
        comm_json = parse_comm_message(db_comm_message_bytes)
        if comm_json is not None:
            if comm_json['destination'] == DBCommProt.DB_DST_GND.value:
                message = process_db_comm_protocol(comm_json, DBDir.DB_TO_UAV)
                if message != "":
                    sendto_tcp_clients(message, _tcp_connections)
            elif comm_json['destination'] == DBCommProt.DB_DST_GND_UAV.value:
                # Always process ping requests right away! Do not wait for UAV response!
                if comm_json['type'] == DBCommProt.DB_TYPE_PING_REQUEST.value:
                    message = process_db_comm_protocol(comm_json, DBDir.DB_TO_UAV)
                    sendto_tcp_clients(message, _tcp_connections)
                    db.sendto_uav(db_comm_message_bytes, DBPort.DB_PORT_COMMUNICATION)
                else:
                    db_log(f"DB_COMM_GND: Destination 2 (GND & UAV) is only supported for ping messages", ident=LOG_WARNING)
                    message = new_error_response_message('Destination 2 (GND & UAV) is unsupported for non ping msgs',
                                                         DBCommProt.DB_ORIGIN_GND.value, comm_json['id'])
                    sendto_tcp_clients(message, _tcp_connections)
            elif comm_json['destination'] == DBCommProt.DB_DST_PER.value:
                db.sendto_uav(db_comm_message_bytes, DBPort.DB_PORT_COMMUNICATION)
            elif comm_json['destination'] == DBCommProt.DB_DST_GCS.value:
                sendto_tcp_clients(db_comm_message_bytes, _tcp_connections)
            elif comm_json['destination'] == DBCommProt.DB_DST_UAV.value:
                db_log("DB_COMM_GND: Forwarding msg to UAV", LOG_DEBUG)
                db.sendto_uav(db_comm_message_bytes, DBPort.DB_PORT_COMMUNICATION)
            else:
                db_log("DB_COMM_GND: Unknown message type", ident=LOG_ERR)
                error_resp = new_error_response_message('DB_COMM_GND: Unknown message type',
                                                        DBCommProt.DB_ORIGIN_GND.value, comm_json['id'])
                sendto_tcp_clients(error_resp, _tcp_connections)
        else:
            db_log("DB_COMM_GND: Corrupt message", ident=LOG_ERR)
            error_resp = new_error_response_message('DB_COMM_GND: Corrupt message', DBCommProt.DB_ORIGIN_GND.value, 0)
            sendto_tcp_clients(error_resp, _tcp_connections)
    except (UnicodeDecodeError, ValueError):
        db_log("DB_COMM_GND: Command could not be processed correctly! (UnicodeDecodeError, ValueError)", ident=LOG_ERR)


def sendto_tcp_clients(data_bytes: bytes, _tcp_connections: list):
    """
    Send to all connected TCP clients

    :param data_bytes: Payload to send
    :param _tcp_connections: List of socket objects to use for sending
    """
    db_log("DB_COMM_GND: Responding ...")
    for connected_socket in _tcp_connections:
        if connected_socket.sendall(data_bytes) is not None:
            db_log("DB_COMM_GND:\tShit!", ident=LOG_ERR)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    parsedArgs = parse_arguments()
    mode = parsedArgs.mode
    list_interfaces = parsedArgs.DB_INTERFACES
    comm_id = bytes([parsedArgs.comm_id])
    comp_mode = parsedArgs.comp_mode
    frame_type = int(parsedArgs.frametype)
    db_log("DB_COMM_GND: Communication ID: " + str(int.from_bytes(comm_id, byteorder='little')) +
           " (" + str(comm_id.hex()) + ")")
    db = DroneBridge(DBDir.DB_TO_UAV, list_interfaces, DBMode.MONITOR, comm_id, DBPort.DB_PORT_COMMUNICATION,
                     tag="DB_COMM_GND", db_blocking_socket=False, frame_type=frame_type, compatibility_mode=comp_mode)
    # We use a stupid tcp implementation where all connected clients receive the data sent by the UAV. GCS must
    # identify the relevant messages based on the id in every communication message. Robust & multiple clients possible
    tcp_master = open_tcp_socket()
    tcp_connections = []
    TCP_BUFFER_SIZE = 2048
    MONITOR_BUFFERSIZE = 2048
    db.clear_socket_buffers()
    prev_seq_num = None

    db_log("DB_COMM_GND: started!")

    read_sockets = [tcp_master]
    read_sockets.extend(db.list_lr_sockets)
    while keep_running:
        try:
            r, w, e = select(read_sockets, [], tcp_connections, 0.5)  # timeout for proper termination?!
            for readable_sock in r:
                if readable_sock is tcp_master:  # new connection
                    conn, addr = readable_sock.accept()
                    tcp_connections.append(conn)
                    read_sockets.append(conn)
                    db_log(f"DB_COMM_GND: TCP client connected {addr}")
                elif readable_sock in tcp_connections:
                    received_data = readable_sock.recv(TCP_BUFFER_SIZE)
                    if len(received_data) == 0:
                        tcp_connections.remove(readable_sock)
                        read_sockets.remove(readable_sock)
                        readable_sock.close()
                        db_log("DB_COMM_GND: Client disconnected")
                    else:
                        process_comm_proto(received_data, tcp_connections)
                elif readable_sock in db.list_lr_sockets:
                    received_data, seq_num = db.parse_packet(bytearray(readable_sock.recv(MONITOR_BUFFERSIZE)))
                    # Prevent processing of the same message received over different network adapters
                    if received_data and (prev_seq_num is None or prev_seq_num is not seq_num):
                        process_comm_proto(received_data, tcp_connections)
                        prev_seq_num = seq_num
                else:
                    db_log("DB_COMM_GND: Unknown socket received something. That should not happen.")
            for error_tcp_sock in e:
                db_log("DB_COMM_GND: A TCP socket encountered an error: " + error_tcp_sock.getpeername(), ident=LOG_ERR)
                tcp_connections.remove(error_tcp_sock)
                try:
                    read_sockets.remove(error_tcp_sock)
                except ValueError:
                    pass  # socket was not among read sockets
                error_tcp_sock.close()
        except InterruptedError:
            keep_running = False
        except error:
            db_log("DB_COMM_GND: Select got an error. That should not happen.", ident=LOG_ERR)
    db.close_sockets()
    for connection in tcp_connections:
        connection.close()
    tcp_master.close()
    db_log("DB_COMM_GND: Terminated")
