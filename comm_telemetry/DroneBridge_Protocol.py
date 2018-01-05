# This file is part of DroneBridge licenced under Apache Licence 2
# https://github.com/seeul8er/DroneBridge/
# Created by Wolfgang Christl

import base64
import json
from socket import *
import select
from subprocess import call

from bpf import attach_filter
from db_comm_messages import change_settings, new_settingsresponse_message, comm_message_extract_info, \
    check_package_good, change_settings_gopro
from db_ip_checker import DB_IP_GETTER


RADIOTAP_HEADER = b'\x00\x00\x0c\x00\x04\x80\x00\x00\x0c\x00\x18\x00'  # 6Mbit transmission speed set with Ralink chips
TO_DRONE = b'\x01'
TO_GROUND = b'\x03'  # v2 ready

DB_PORT_CONTROLLER = b'\x01'
DB_PORT_TELEMETRY = b'\x02'
DB_PORT_VIDEO = b'\x03'
DB_PORT_COMMUNICATION = b'\x04'
DB_PORT_STATUS = b'\x05'
DB_PORT_PROXY = b'\x06'
DB_PORT_RC = b'\x07'

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

    def __init__(self, src_mac, udp_port_rx, ip_rx, udp_port_smartphone, comm_direction, interface_drone_comm,
                 mode, communication_id, dronebridge_port):
        self.src_mac = src_mac
        self.comm_id = communication_id  # must be the same on drone and groundstation
        self.udp_port_rx = udp_port_rx  # 1604
        self.ip_rx = ip_rx
        self.udp_port_smartphone = udp_port_smartphone  # we bind to that locally
        self.comm_direction = comm_direction  # set to 0x01 if program runs on groundst. and to 0x02 if runs on drone
        self.interface = interface_drone_comm  # the long range interface
        self.mode = mode
        self.tag = ''
        if dronebridge_port == DB_PORT_TELEMETRY:
            self.tag = "DB_TEL: "
        elif dronebridge_port == DB_PORT_COMMUNICATION:
            self.tag = "DB_Comm: "
        if self.mode == 'wifi':
            self.short_mode = 'w'
        else:
            self.short_mode = 'm'
        self.fcf = b'\xb4\x00\x00\x00'  # RTS frames
        self.db_port = dronebridge_port
        self.comm_sock = self._open_comm_sock()
        if self.comm_direction == TO_DRONE:
            self.android_sock = self._open_android_udpsocket()
            self.ipgetter = DB_IP_GETTER()
        self.changed = False
        self.signal = 0  # signal quality that is measured [dBm]
        self.first_run = True
        self.seq_num = 0

    def receive_datafromdrone(self):
        """Check if new data from the drone arrived and return packet payload"""
        if self.mode == 'wifi':
            try:
                data, addr = self.comm_sock.recvfrom(UDP_BUFFERSIZE)
                return data
            except Exception as e:
                print(self.tag + str(e) + ": Drone is not ready or has wrong IP address of groundstation. Sending hello-packet")
                self._send_hello()
                return False
        else:
            try:
                readable, writable, exceptional = select.select([self.comm_sock], [], [], 1.5)
                if readable:
                    data = self._pars_packet(bytearray(self.comm_sock.recv(MONITOR_BUFFERSIZE_COMM)))
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
                print(self.tag + str(e) + ": Drone is not ready or has wrong IP address of groundstation. Sending hello-packet")
                self._send_hello()
                return False
        else:
            try:
                while True:
                    data = self._pars_packet(bytearray(self.comm_sock.recv(MONITOR_BUFFERSIZE)))
                    if data != False:
                        return data
            except Exception as e:
                print(self.tag + str(e) + ": Error receiving telemetry form drone (monitor mode)")
                return False

    def receive_process_datafromgroundstation(self):
        """Check if new data from the groundstation arrived and process the packet"""
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
            if self.db_port == DB_PORT_TELEMETRY:
                # socket is non-blocking - return if nothing there and keep sending telemetry
                readable, writable, exceptional = select.select([self.comm_sock], [], [], 0)
                if readable:
                    # just get RSSI of radiotap header
                    self._pars_packet(bytes(self.comm_sock.recv(MONITOR_BUFFERSIZE)))
            else:
                if self.first_run:
                    self._clear_monitor_comm_socket_buffer()
                    self.first_run = False
                db_comm_prot_request = self._pars_packet(bytes(self.comm_sock.recv(MONITOR_BUFFERSIZE_COMM)))
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
                    print(self.tag + "Could not send to smartphone ("+self.ip_smartp+"). Make sure it is connected.")
                    return 0

    def sendto_groundstation(self, data_bytes, port_bytes):
        """Call this function to send stuff to the groundstation or directly to smartphone"""
        if self.mode == "wifi":
            num = self._sendto_tx_wifi(data_bytes)
        else:
            num = self._send_monitor(data_bytes, port_bytes, TO_GROUND)
        return num

    def send_beacon(self):
        self._sendto_drone('groundstation_beacon'.encode(), DB_PORT_TELEMETRY)

    def update_routing_gopro(self):
        print(self.tag + "Update iptables to send GoPro stream to " + str(self.ip_rx))
        if self.changed:
            call("iptables -t nat -R PREROUTING 1 -p udp --dport 8554 -j DNAT --to " + str(self.ip_rx))
        else:
            call("iptables -t nat -I PREROUTING 1 -p udp --dport 8554 -j DNAT --to " + str(self.ip_rx))
            self.changed = True

    def getsmartphonesocket(self):
        return self.android_sock

    def getcommsocket(self):
        return self.comm_sock

    def _pars_packet(self, packet):
        """Pars DroneBridge raw protocol v2. Returns False if not OK or return packet payload if it is!"""
        rth_length = packet[2]
        db_v2_payload_length = int.from_bytes(packet[(rth_length + 7):(rth_length + 8)] +
                                              packet[(rth_length + 8):(rth_length + 9)],
                                              byteorder='little', signed=False)
        payload_start = rth_length + DB_V2_HEADER_LENGTH
        return packet[payload_start:(payload_start+db_v2_payload_length)]

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
        """Routing of the DroneBridge communication protocol packets"""
        status = False
        extracted_info = comm_message_extract_info(raw_data_encoded)  # returns json bytes [0] and crc bytes [1]
        try:
            loaded_json = json.loads(extracted_info[0].decode())
        except UnicodeDecodeError:
            print(self.tag + "Invalid command: Could not decode json message")
            return False

        if loaded_json['destination'] == 1 and self.comm_direction == TO_DRONE and check_package_good(extracted_info):
            message = self._process_db_comm_protocol_type(loaded_json)
            if message != "":
                status = self.sendto_smartphone(message, self.APP_PORT_COMM)
            else:
                status = True
        elif loaded_json['destination'] == 2 and check_package_good(extracted_info):
            if self.comm_direction == TO_DRONE:
                response_drone = self._redirect_comm_to_drone(raw_data_encoded)
                if response_drone != False and response_drone!=None:
                    message = self._process_db_comm_protocol_type(loaded_json)
                    self.sendto_smartphone(message, self.APP_PORT_COMM)
                    status = self.sendto_smartphone(response_drone, self.APP_PORT_COMM)
            else:
                message = self._process_db_comm_protocol_type(loaded_json)
                sentbytes = self.sendto_groundstation(message, DB_PORT_COMMUNICATION)
                if sentbytes == None:
                    status = True
        elif loaded_json['destination'] == 3:
            if self.comm_direction == TO_DRONE:
                status = self._sendto_drone(raw_data_encoded, DB_PORT_COMMUNICATION)
            else:
                change_settings_gopro(loaded_json)
        elif loaded_json['destination'] == 4:
            if self.comm_direction == TO_DRONE:
                status = self.sendto_smartphone(raw_data_encoded, self.APP_PORT_COMM)
        else:
            print(self.tag + "DB_COMM_PROTO: Unknown message destination")
        return status

    def _process_db_comm_protocol_type(self, loaded_json):
        """Execute the command given in the DroneBridge communication packet"""
        message = ""
        if loaded_json['type'] == 'mspcommand':
            # deprecated
            self._sendto_drone(base64.b64decode(loaded_json['MSP']), DB_PORT_CONTROLLER)
        elif loaded_json['type'] == 'settingsrequest':
            if self.comm_direction == TO_DRONE:
                message = new_settingsresponse_message(loaded_json, 'groundstation')
            else:
                message = new_settingsresponse_message(loaded_json, 'drone')
        elif loaded_json['type'] == 'settingschange':
            if self.comm_direction == TO_DRONE:
                message = change_settings(loaded_json, 'groundstation')
            else:
                message = change_settings(loaded_json, 'drone')
        else:
            print(self.tag + "DB_COMM_PROTO: Unknown message type")
        return message

    def _redirect_comm_to_drone(self, raw_data_encoded):
        """This one will forward communication message to drone. Response is returned or False"""
        if self.first_run:
            self._clear_monitor_comm_socket_buffer()
            self.first_run = False
        self._sendto_drone(raw_data_encoded, DB_PORT_COMMUNICATION)
        response = self.receive_datafromdrone()
        print(self.tag + "Parsed packet received from drone:")
        print(response)
        return response

    def _send_hello(self):
        """Send this in wifi mode to let the drone know about IP of groundstation"""
        self.comm_sock.sendto("tx_hello_packet".encode(), (self.ip_rx, self.udp_port_rx))

    def _sendto_drone(self, data_bytes, port_bytes):
        """Call this function to send stuff to the drone!"""
        if self.mode == "wifi":
            num = self._sendto_rx_wifi(data_bytes, port_bytes)
        else:
            num = self._send_monitor(data_bytes, port_bytes, TO_DRONE)
        return num

    def _sendto_tx_wifi(self, data_bytes):
        """Sends LTM and other stuff to groundstation/smartphone in wifi mode"""
        while True:
            r, w, e = select.select([], [self.comm_sock], [], 0)
            if w:
                num = self.comm_sock.sendto(data_bytes, (self.ip_rx, self.udp_port_rx))
                return num

    def _sendto_rx_wifi(self, raw_data_bytes, port_bytes):
        """
        Send a packet to drone in wifi mode
        depending on message type different ports/programmes aka front ends on the drone need to be addressed
        """
        if port_bytes == DB_PORT_CONTROLLER:
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
                num = self.comm_sock.sendall(RADIOTAP_HEADER + db_v2_raw_header + data_bytes)
                break
        return num

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
        if self.comm_direction == b'\x00':
            sock.settimeout(1)
        else:
            sock.setblocking(False)
        return sock

    def _open_comm_monitorsocket(self):
        print(self.tag + "Opening socket for monitor mode")
        raw_socket = socket(AF_PACKET, SOCK_RAW, htons(0x0004))
        raw_socket.bind((self.interface, 0))
        raw_socket = self._set_comm_socket_behavior(raw_socket)
        if self.comm_direction == TO_GROUND:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=TO_DRONE,
                                       byte_port=self.db_port)  # filter for packets TO_DRONE
        else:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=TO_GROUND,
                                       byte_port=self.db_port)  # filter for packets TO_GROUND
        return raw_socket

    def _set_comm_socket_behavior(self, thesocket):
        """Set to blocking or non-blocking depending on Module (Telemetry, Communication) and if on drone or ground"""
        adjusted_socket = thesocket
        if self.comm_direction == TO_GROUND and self.db_port == DB_PORT_TELEMETRY:  # On drone side in telemetry module
            adjusted_socket.setblocking(False)
        elif self.comm_direction == TO_DRONE and self.db_port == DB_PORT_COMMUNICATION:  # On ground side in comm module
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
        print(self.tag + "Opening UDP-Socket to smartphone on port: "+str(self.udp_port_smartphone))
        sock = socket(AF_INET, SOCK_DGRAM)
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
        if self.db_port == DB_PORT_COMMUNICATION:
            address = ('', self.udp_port_smartphone)
            sock.bind(address)
        sock.setblocking(False)
        return sock
