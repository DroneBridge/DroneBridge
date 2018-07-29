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

import base64
import json
from enum import Enum
from socket import *
import select
from subprocess import call

from DBCommProt import DBCommProt
from bpf import attach_filter
from db_comm_messages import change_settings, new_settingsresponse_message, comm_message_extract_info, \
    comm_crc_correct, change_settings_gopro, create_sys_ident_response, new_error_response_message, \
    new_ping_response_message, new_ack_message, change_cam_selection, init_cam_gpios, normalize_jscal_axis
from db_ip_checker import DB_IP_GETTER


class DBPort(Enum):
    DB_PORT_CONTROLLER = b'\x01'
    DB_PORT_TELEMETRY = b'\x02'
    DB_PORT_VIDEO = b'\x03'
    DB_PORT_COMMUNICATION = b'\x04'
    DB_PORT_STATUS = b'\x05'
    DB_PORT_PROXY = b'\x06'
    DB_PORT_RC = b'\x07'


class DBDir(Enum):
    DB_TO_UAV = b'\x01'
    DB_TO_GND = b'\x03'


RADIOTAP_HEADER = b'\x00\x00\x0c\x00\x04\x80\x00\x00\x0c\x00\x18\x00'  # 6Mbit transmission speed set with Ralink chips
ETH_TYPE = b"\x88\xAB"
DB_V2_HEADER_LENGTH = 10
DRIVER_ATHEROS = "atheros"
DRIVER_RALINK = "ralink"
UDP_BUFFERSIZE = 2048
MONITOR_BUFFERSIZE = 2048
MONITOR_BUFFERSIZE_COMM = 2048


class DBProtocol:
    ip_smartp = "192.168.42.129"
    APP_PORT_TEL = 1604
    APP_PORT_COMM = 1603

    def __init__(self, udp_port_rx, ip_rx, udp_port_smartphone, comm_direction, interface_drone_comm,
                 mode, communication_id, dronebridge_port, tag=''):
        if type(communication_id) is int:
            self.comm_id = bytes([communication_id])  # must be the same on drone and groundstation
        else:
            self.comm_id = communication_id  # must be the same on drone and groundstation
        assert type(self.comm_id) is bytes
        self.udp_port_rx = udp_port_rx  # 1604
        self.ip_rx = ip_rx
        self.udp_port_smartphone = udp_port_smartphone  # we bind to that locally
        # direction is stored as DBDir
        self.comm_direction = comm_direction  # set to 0x01 if program runs on groundst. and to 0x03 if runs on drone
        assert type(self.comm_direction) is DBDir
        self.interface = interface_drone_comm  # the long range interface
        self.mode = mode
        self.tag = tag
        if self.mode == 'wifi':
            self.short_mode = 'w'
        else:
            self.short_mode = 'm'
        self.fcf = b'\xb4\x00\x00\x00'  # RTS frames
        # port is stored as byte value
        if type(dronebridge_port) is DBPort:
            self.db_port = dronebridge_port.value
        else:
            self.db_port = dronebridge_port
        assert type(self.db_port) is bytes
        self.comm_sock = self._open_comm_sock()
        # dirty fix till we do some proper code cleanup!
        if self.comm_direction == DBDir.DB_TO_UAV and (self.db_port == DBPort.DB_PORT_TELEMETRY.value or
                                                       self.db_port == DBPort.DB_PORT_COMMUNICATION.value):
            self.android_sock = self._open_android_udpsocket()
            self.ipgetter = DB_IP_GETTER()
        self.changed = False
        self.signal = 0  # signal quality that is measured [dBm]
        self.first_run = True
        self.seq_num = 0
        init_cam_gpios()

    def receive_from_db(self, custom_timeout=1.5):
        """Check if new data from the drone arrived and return packet payload. Default timeout is 1.5s"""
        if self.mode == 'wifi':
            try:
                data, addr = self.comm_sock.recvfrom(UDP_BUFFERSIZE)
                return data
            except Exception as e:
                print(
                    self.tag + str(e) + ": Drone is not ready or has wrong IP address of groundstation. Sending hello")
                self._send_hello()
                return False
        else:
            try:
                readable, writable, exceptional = select.select([self.comm_sock], [], [], custom_timeout)
                if readable:
                    data = self.parse_packet(bytearray(self.comm_sock.recv(MONITOR_BUFFERSIZE_COMM)))
                    if data != False:
                        return data
            except timeout as t:
                print(self.tag + str(t) + "Socket timed out. No response received from drone (monitor mode)")
                return False
            except Exception as e:
                print(self.tag + str(e) + ": Error receiving data form drone (monitor mode)")
                return False

    def receive_telemetryfromdrone(self):
        """Receive telemetry message from drone on groundstation and return the payload."""
        if self.mode == 'wifi':
            try:
                data, addr = self.comm_sock.recvfrom(UDP_BUFFERSIZE)
                return data
            except Exception as e:
                print(
                    self.tag + str(e) + ": Drone is not ready or has wrong IP address of groundstation. Sending hello")
                self._send_hello()
                return False
        else:
            try:
                while True:
                    data = self.parse_packet(bytearray(self.comm_sock.recv(MONITOR_BUFFERSIZE)))
                    if data != False:
                        return data
            except Exception as e:
                print(self.tag + str(e) + ": Error receiving telemetry form drone (monitor mode)")
                return False

    def receive_process_datafromgroundstation(self):
        """Check if new data from the groundstation arrived and process the packet - do not use for custom data!"""
        # check if the socket received something and process data
        if self.mode == "wifi":
            readable, writable, exceptional = select.select([self.comm_sock], [], [], 0)
            if readable:
                data, addr = self.comm_sock.recvfrom(UDP_BUFFERSIZE)
                if data.decode() == "tx_hello_packet":
                    self.ip_rx = addr[0]
                    print(self.tag + "Updated goundstation IP-address to: " + str(self.ip_rx))
                else:
                    print(self.tag + "New data from groundstation: " + data.decode())
        else:
            if self.db_port == DBPort.DB_PORT_TELEMETRY.value:
                # socket is non-blocking - return if nothing there and keep sending telemetry
                readable, writable, exceptional = select.select([self.comm_sock], [], [], 0)
                if readable:
                    # just get RSSI of radiotap header
                    self.parse_packet(bytes(self.comm_sock.recv(MONITOR_BUFFERSIZE)))
            else:
                if self.first_run:
                    self._clear_monitor_comm_socket_buffer()
                    self.first_run = False
                db_comm_prot_request = self.parse_packet(bytes(self.comm_sock.recv(MONITOR_BUFFERSIZE_COMM)))
                if db_comm_prot_request != False:
                    try:
                        if not self._route_db_comm_protocol(db_comm_prot_request):
                            print(self.tag + "smartphone request could not be processed correctly")
                    except (UnicodeDecodeError, ValueError):
                        print(self.tag + "Received message from groundstation with error. Not UTF error or ValueError")

    def process_smartphonerequests(self, last_keepalive):
        """See if smartphone told the groundstation to do something. Returns recent keep-alive time"""
        r, w, e = select.select([self.android_sock], [], [], 0)
        if r:
            smartph_data, android_addr = self.android_sock.recvfrom(UDP_BUFFERSIZE)
            return self._process_smartphone_command(smartph_data, last_keepalive)
        return last_keepalive

    def sendto_smartphone(self, raw_data, port):
        """Sends data to smartphone. Socket is nonblocking so we need to wait till it becomes"""
        self.ip_smartp = self.ipgetter.return_smartphone_ip()
        while True:
            r, w, e = select.select([], [self.android_sock], [], 0)
            if w:
                try:
                    return self.android_sock.sendto(raw_data, (self.ip_smartp, port))
                except:
                    print(
                        self.tag + "Could not send to smartphone (" + self.ip_smartp + "). Make sure it is connected.")
                    return 0

    def sendto_groundstation(self, data_bytes, db_port):
        """Call this function to send stuff to the groundstation"""
        if type(db_port) is DBPort:
            db_port = db_port.value
        if self.mode == "wifi":
            num = self._sendto_tx_wifi(data_bytes)
        else:
            num = self._send_monitor(data_bytes, db_port, DBDir.DB_TO_GND.value)
        return num

    def sendto_uav(self, data_bytes, db_port):
        """Call this function to send stuff to the drone!"""
        if type(db_port) is DBPort:
            db_port = db_port.value
        if self.mode == "wifi":
            num = self._sendto_rx_wifi(data_bytes, db_port)
        else:
            num = self._send_monitor(data_bytes, db_port, DBDir.DB_TO_UAV.value)
        return num

    def send_beacon(self):
        self.sendto_uav('groundstation_beacon'.encode(), DBPort.DB_PORT_TELEMETRY.value)

    def update_routing_gopro(self):
        print(self.tag + "Update iptables to send GoPro stream to " + str(self.ip_rx))
        if self.changed:
            call("iptables -t nat -R PREROUTING 1 -p udp --dport 8554 -j DNAT --to " + str(self.ip_rx))
        else:
            call("iptables -t nat -I PREROUTING 1 -p udp --dport 8554 -j DNAT --to " + str(self.ip_rx))
            self.changed = True

    def set_raw_sock_blocking(self, is_blocking):
        self.comm_sock.setblocking(is_blocking)

    def getsmartphonesocket(self):
        return self.android_sock

    def getcommsocket(self):
        return self.comm_sock

    @staticmethod
    def parse_packet(packet):
        """Pars DroneBridge raw protocol v2. Returns False if not OK or return packet payload if it is!"""
        rth_length = packet[2]
        db_v2_payload_length = int.from_bytes(packet[(rth_length + 7):(rth_length + 8)] +
                                              packet[(rth_length + 8):(rth_length + 9)],
                                              byteorder='little', signed=False)
        payload_start = rth_length + DB_V2_HEADER_LENGTH
        return packet[payload_start:(payload_start + db_v2_payload_length)]

    def _process_smartphone_command(self, raw_data, thelast_keepalive):
        """We received something from the smartphone. Most likely a communication message. Do something with it."""
        try:
            raw_data_decoded = bytes.decode(raw_data)
            print(self.tag + "Received from SP: " + raw_data_decoded)
        except UnicodeDecodeError:
            pass
        if not self._route_db_comm_protocol(raw_data):
            print(self.tag + "smartphone command could not be processed correctly!")
        return thelast_keepalive

    def _route_db_comm_protocol(self, raw_data_encoded):
        """Routing of the DroneBridge communication protocol packets. Only write to local settings if we get a positive
        response from the drone! Ping requests are a exception!"""
        status = False
        extracted_info = comm_message_extract_info(raw_data_encoded)  # returns json bytes [0] and crc bytes [1]
        try:
            loaded_json = json.loads(extracted_info[0].decode())
        except UnicodeDecodeError:
            print(self.tag + "Invalid command: Could not decode json message")
            return False
        except ValueError:
            print(self.tag + "ValueError on decoding extracted_info[0]")
            return False

        # Check CRC
        if not comm_crc_correct(extracted_info):
            message = new_error_response_message('Bad CRC', self.comm_direction.value,
                                                 loaded_json['id'])
            if self.comm_direction == DBDir.DB_TO_UAV:
                self.sendto_smartphone(message, DBPort.DB_PORT_COMMUNICATION.value)
            else:
                self.sendto_groundstation(message, DBPort.DB_PORT_COMMUNICATION.value)
            return False

        # Process communication protocol
        if loaded_json['destination'] == 1 and self.comm_direction == DBDir.DB_TO_UAV:
            message = self._process_db_comm_protocol_type(loaded_json)
            if message != "":
                status = self.sendto_smartphone(message, self.APP_PORT_COMM)
            else:
                status = True
        elif loaded_json['destination'] == 2:
            if self.comm_direction == DBDir.DB_TO_UAV:
                # Always process ping requests right away! Do not wait for UAV response!
                if loaded_json['type'] == DBCommProt.DB_TYPE_PING_REQUEST.value:
                    message = self._process_db_comm_protocol_type(loaded_json)
                    status = self.sendto_smartphone(message, self.APP_PORT_COMM)
                    response_drone = self._redirect_comm_to_drone(raw_data_encoded)
                    if type(response_drone) is bytearray:
                        status = self.sendto_smartphone(response_drone, self.APP_PORT_COMM)
                else:
                    response_drone = self._redirect_comm_to_drone(raw_data_encoded)
                    if type(response_drone) is bytearray:
                        message = self._process_db_comm_protocol_type(loaded_json)
                        self.sendto_smartphone(message, self.APP_PORT_COMM)
                        status = self.sendto_smartphone(response_drone, self.APP_PORT_COMM)
                    else:
                        message = new_error_response_message('UAV was unreachable - command not executed',
                                                             DBCommProt.DB_ORIGIN_GND.value, loaded_json['id'])
                        self.sendto_smartphone(message, self.APP_PORT_COMM)
            else:
                message = self._process_db_comm_protocol_type(loaded_json)
                sentbytes = self.sendto_groundstation(message, DBPort.DB_PORT_COMMUNICATION.value)
                if sentbytes == None:
                    status = True
        elif loaded_json['destination'] == 3:
            if self.comm_direction == DBDir.DB_TO_UAV:
                status = self.sendto_uav(raw_data_encoded, DBPort.DB_PORT_COMMUNICATION.value)
            else:
                change_settings_gopro(loaded_json)
        elif loaded_json['destination'] == 4:
            if self.comm_direction == DBDir.DB_TO_UAV:
                status = self.sendto_smartphone(raw_data_encoded, self.APP_PORT_COMM)
        elif loaded_json['destination'] == 5:
            if self.comm_direction == DBDir.DB_TO_UAV:
                status = self.sendto_uav(raw_data_encoded, DBPort.DB_PORT_COMMUNICATION.value)
            else:
                message = self._process_db_comm_protocol_type(loaded_json)
                if self.sendto_groundstation(message, DBPort.DB_PORT_COMMUNICATION.value) == None:
                    status = True
        else:
            print(self.tag + "DB_COMM_PROTO: Unknown message destination")
        return status

    def _process_db_comm_protocol_type(self, loaded_json):
        """Execute the command given in the DroneBridge communication packet"""
        message = ""
        if loaded_json['type'] == DBCommProt.DB_TYPE_MSP.value:
            # deprecated
            self.sendto_uav(base64.b64decode(loaded_json['MSP']), DBPort.DB_PORT_CONTROLLER.value)
        elif loaded_json['type'] == DBCommProt.DB_TYPE_SETTINGS_REQUEST.value:
            if self.comm_direction == DBDir.DB_TO_UAV:
                message = new_settingsresponse_message(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
            else:
                message = new_settingsresponse_message(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
        elif loaded_json['type'] == DBCommProt.DB_TYPE_SETTINGS_CHANGE.value:
            if self.comm_direction == DBDir.DB_TO_UAV:
                message = change_settings(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
            else:
                message = change_settings(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
        elif loaded_json['type'] == DBCommProt.DB_TYPE_SYS_IDENT_REQUEST.value:
            if self.comm_direction == DBDir.DB_TO_UAV:
                message = create_sys_ident_response(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
            else:
                message = create_sys_ident_response(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
        elif loaded_json['type'] == DBCommProt.DB_TYPE_PING_REQUEST.value:
            if self.comm_direction == DBDir.DB_TO_UAV:
                message = new_ping_response_message(loaded_json, DBCommProt.DB_ORIGIN_GND.value)
            else:
                message = new_ping_response_message(loaded_json, DBCommProt.DB_ORIGIN_UAV.value)
        elif loaded_json['type'] == DBCommProt.DB_TYPE_CAMSELECT.value:
            change_cam_selection(loaded_json['cam'])
            message = new_ack_message(DBCommProt.DB_ORIGIN_UAV.value, loaded_json['id'])
        elif loaded_json['type'] == DBCommProt.DB_TYPE_ADJUSTRC.value:
            normalize_jscal_axis(loaded_json['device'])
            message = new_ack_message(DBCommProt.DB_ORIGIN_GND.value, loaded_json['id'])
        else:
            if self.comm_direction == DBDir.DB_TO_UAV:
                message = new_error_response_message('unsupported message type', DBCommProt.DB_ORIGIN_GND.value,
                                                     loaded_json['id'])
            else:
                message = new_error_response_message('unsupported message type', DBCommProt.DB_ORIGIN_UAV.value,
                                                     loaded_json['id'])
            print(self.tag + "DB_COMM_PROTO: Unknown message type")
        return message

    def _redirect_comm_to_drone(self, raw_data_encoded):
        """This one will forward communication message to drone. Response is returned or False"""
        if self.first_run:
            self._clear_monitor_comm_socket_buffer()
            self.first_run = False
        self.sendto_uav(raw_data_encoded, DBPort.DB_PORT_COMMUNICATION.value)
        response = self.receive_from_db(custom_timeout=0.3)
        print(self.tag + "Parsed packet received from drone:")
        print(response)
        return response

    def _send_hello(self):
        """Send this in wifi mode to let the drone know about IP of groundstation"""
        self.comm_sock.sendto("tx_hello_packet".encode(), (self.ip_rx, self.udp_port_rx))

    def _sendto_tx_wifi(self, data_bytes):
        """Sends LTM and other stuff to groundstation/smartphone in wifi mode"""
        while True:
            r, w, e = select.select([], [self.comm_sock], [], 0)
            if w:
                return self.comm_sock.sendto(data_bytes, (self.ip_rx, self.udp_port_rx))

    def _sendto_rx_wifi(self, raw_data_bytes, port_bytes):
        """
        Send a packet to drone in wifi mode
        depending on message type different ports/programmes aka front ends on the drone need to be addressed
        """
        if port_bytes == DBPort.DB_PORT_CONTROLLER.value:
            print(self.tag + "Sending MSP command to RX Controller (wifi)")
            try:
                # TODO
                num = 0
                pass
            except Exception:
                return False
            print(self.tag + "Sent it!")
        else:
            print(self.tag + "Sending a message to telemetry frontend on drone")
            num = self.comm_sock.sendto(raw_data_bytes, (self.ip_rx, self.udp_port_rx))
        return num

    def _send_monitor(self, data_bytes, port_bytes, direction):
        """Send a packet in monitor mode using DroneBridge raw protocol v2. Return None on success"""
        payload_length_bytes = bytes(len(data_bytes).to_bytes(2, byteorder='little', signed=False))
        if self.seq_num == 255:
            self.seq_num = 0
        else:
            self.seq_num += 1
        db_v2_raw_header = bytes(bytearray(self.fcf + direction + self.comm_id + port_bytes + payload_length_bytes +
                                           bytes([self.seq_num])))
        while True:
            r, w, e = select.select([], [self.comm_sock], [], 0)
            if w:
                return self.comm_sock.sendall(RADIOTAP_HEADER + db_v2_raw_header + data_bytes)

    def _open_comm_sock(self):
        """Opens a socket that talks to drone (on tx side) or groundstation (on rx side)"""
        if self.mode == "wifi":
            return self._open_comm_udpsocket()
        else:
            return self._open_comm_monitorsocket()

    def _open_comm_udpsocket(self):
        print(self.tag + "Opening UDP-Socket for DroneBridge communication")
        sock = socket(AF_INET, SOCK_DGRAM)
        server_address = ('', self.udp_port_rx)
        sock.bind(server_address)
        if self.comm_direction.value == b'\x00':
            sock.settimeout(1)
        else:
            sock.setblocking(False)
        return sock

    def _open_comm_monitorsocket(self):
        print(self.tag + "Opening socket for monitor mode")
        raw_socket = socket(AF_PACKET, SOCK_RAW, htons(0x0004))
        raw_socket.bind((self.interface, 0))
        raw_socket = self._set_comm_socket_behavior(raw_socket)
        if self.comm_direction == DBDir.DB_TO_GND:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=DBDir.DB_TO_UAV.value,
                                       byte_port=self.db_port)  # filter for packets TO_DRONE
        else:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=DBDir.DB_TO_GND.value,
                                       byte_port=self.db_port)  # filter for packets TO_GROUND
        return raw_socket

    def _set_comm_socket_behavior(self, thesocket):
        """Set to blocking or non-blocking depending on Module (Telemetry, Communication) and if on drone or ground"""
        adjusted_socket = thesocket
        # On drone side in telemetry module
        if self.comm_direction == DBDir.DB_TO_GND and self.db_port == DBPort.DB_PORT_TELEMETRY.value:
            adjusted_socket.setblocking(False)
        # On ground side in comm module
        elif self.comm_direction == DBDir.DB_TO_UAV and self.db_port == DBPort.DB_PORT_COMMUNICATION.value:
            adjusted_socket.setblocking(False)
        return adjusted_socket

    def _clear_monitor_comm_socket_buffer(self):
        self.comm_sock.setblocking(False)
        while True:
            readable, writable, exceptional = select.select([self.comm_sock], [], [], 1)
            if readable:
                self.comm_sock.recv(8192)
            else:
                break
        self.comm_sock.setblocking(True)

    def _open_android_udpsocket(self):
        print(self.tag + "Opening UDP-Socket to smartphone on port: " + str(self.udp_port_smartphone))
        sock = socket(AF_INET, SOCK_DGRAM)
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
        if self.db_port == DBPort.DB_PORT_COMMUNICATION.value:
            address = ('', self.udp_port_smartphone)
            sock.bind(address)
        sock.setblocking(False)
        return sock
