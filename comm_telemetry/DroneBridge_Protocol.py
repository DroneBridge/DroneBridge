import base64
import json
from socket import *
import crc8
import time
import select
import psutil
from subprocess import call

from bpf import attach_filter
from db_comm_messages import change_settings, new_settingsresponse_message, comm_message_extract_info, \
    check_package_good, change_settings_gopro
from db_ip_checker import DB_IP_GETTER

RADIOTAP_HEADER = b'\x00\x00\x0c\x00\x04\x80\x00\x00\x0c\x00\x18\x00'  # 6Mbit transmission speed set with Ralink chips
DB_FRAME_VERSION = b'\x01'
TO_DRONE = b'\x01'
TO_GROUND = b'\x02'
PORT_CONTROLLER = b'\x01'
PORT_TELEMETRY = b'\x02'
PORT_VIDEO = b'\x03'
PORT_COMMUNICATION = b'\x04'
ETH_TYPE = b"\x88\xAB"
DB_80211_HEADER_LENGTH = 24
DRIVER_ATHEROS = "atheros"
DRIVER_RALINK = "ralink"
UDP_BUFFERSIZE = 512
MONITOR_BUFFERSIZE = 128
MONITOR_BUFFERSIZE_COMM = 2048


class DBProtocol:
    ip_smartp = "192.168.42.129"
    LTM_PORT_SMARTPHONE = 1604
    COMM_PORT_SMARTPHONE = 1603

    def __init__(self, src_mac, dst_mac, udp_port_rx, ip_rx, udp_port_smartphone, comm_direction, interface_drone_comm,
                 mode, communication_id, frame_type, dronebridge_port):
        self.src_mac = src_mac
        self.dst_mac = dst_mac  # not used in monitor mode
        self.comm_id = communication_id  # must be the same on drone and groundstation
        self.udp_port_rx = udp_port_rx  # 1604
        self.ip_rx = ip_rx
        self.udp_port_smartphone = udp_port_smartphone  # we bind to that locally
        # communication direction: the direction the packets will have when sent from the application
        self.comm_direction = comm_direction  # set to 0x01 if program runs on groundst. and to 0x02 if runs on drone
        self.interface = interface_drone_comm  # the long range interface
        self.mode = mode
        self.tag = ''
        if dronebridge_port == PORT_TELEMETRY:
            self.tag = "DB_TEL: "
        elif dronebridge_port == PORT_COMMUNICATION:
            self.tag = "DB_Comm: "
        if self.mode == 'wifi':
            self.short_mode = 'w'
        else:
            self.short_mode = 'm'
        if frame_type == '1':
            self.fcf = b'\x08\x00'
            self.driver = DRIVER_RALINK
        else:
            self.fcf = b'\x80\x00'
            self.driver = DRIVER_ATHEROS
        self.db_port = dronebridge_port
        self.comm_sock = self._open_comm_sock()
        if self.comm_direction == TO_DRONE:
            self.android_sock = self._open_android_udpsocket()
        self.changed = False
        self.signal = 0  # signal quality that is measured [dBm]
        self.ipgetter = DB_IP_GETTER()
        self.first_run = True

    def receive_datafromdrone(self):
        """Used by db_comm_protocol - want non-blocking socket to be able to set timeout in this case"""
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
        """Used by db_telemetry_tx - want blocking socket in this case"""
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
        # check if the socket received something and process data
        if self.mode == "wifi":
            readable, writable, exceptional = select.select([self.comm_sock], [], [], 0)
            if readable:
                data, addr = self.comm_sock.recvfrom(UDP_BUFFERSIZE)
                if data.decode() == "tx_hello_packet":
                    self.ip_rx = addr[0]
                    self.updateRouting()
                    print(self.tag + "Updated goundstation IP-address to: " + str(self.ip_rx))
                else:
                    print(self.tag + "New data from groundstation: " + data.decode())
        else:
            if self.db_port == PORT_TELEMETRY:
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
                        print("Received from groundstation")
                        print(db_comm_prot_request)
                        if not self._route_db_comm_protocol(db_comm_prot_request):
                            print(self.tag + "smartphone request could not be processed correctly")
                    except UnicodeDecodeError as e:
                        print(self.tag+ "Received message not UTF-8 conform. Maybe a invalid packet in the buffer.")

    def process_smartphonerequests(self, last_keepalive):
        """See if smartphone told the groundstation to do something. Returns recent keep-alive time"""
        r, w, e = select.select([self.android_sock], [], [], 0)
        if r:
            smartph_data, android_addr = self.android_sock.recvfrom(UDP_BUFFERSIZE)
            return self._process_smartphonecommand(smartph_data, last_keepalive)
        return last_keepalive

    def check_smartphone_ready(self):
        """Checks if smartphone app is ready for data. Returns IP of smartphone"""
        sock_status = select.select([self.android_sock], [], [], 0.05)
        if sock_status[0]:
            new_data, new_addr = self.android_sock.recvfrom(UDP_BUFFERSIZE)
            if new_data.decode() == "smartphone_is_still_here":
                print(self.tag + "Smartphone is ready")
                self.ip_smartp = new_addr[0]
                print(self.tag + "(IGNORED) Sending future data to smartphone - " + self.ip_smartp + ":" + str(self.udp_port_smartphone))
                return True
        return False

    def finish_dronebridge_ltmframe(self, frame):
        """Adds information to custom LTM-Frame on groundstation side"""
        if self.mode == 'wifi':
            with open('/proc/net/wireless') as fp:
                for line in fp:
                    if line.startswith(self.interface, 1, len(self.interface) + 1):
                        result = line.split(" ", 8)
                        frame[5] = int(result[5][:-1])
                        frame[6] = int(result[7][1:-1])
                        fp.close()
                        return bytes(frame)
            return frame
        else:
            # frame[5] = int((int(self.datarate)*500)/1000)
            frame[6] = self.signal
            # TODO add wbc information and crc
            return bytes(frame)

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

    def send_dronebridge_frame(self):
        DroneBridgeFrame = b'$TY' + self.short_mode.encode() + chr(int(psutil.cpu_percent(interval=None))).encode() + \
                           bytes([self.signal]) + b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
        self.sendto_groundstation(DroneBridgeFrame, PORT_TELEMETRY)

    def send_beacon(self):
        self._sendto_drone('groundstation_beacon'.encode(), PORT_TELEMETRY)

    def updateRouting(self):
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
        """Check if packet is OK and get RSSI. Returns False if not OK or return packet payload if it is"""
        rth_length = packet[2]
        payload_length = int.from_bytes(packet[(rth_length + 19):(rth_length + 20)] + packet[(rth_length + 20):(rth_length + 21)], byteorder='little', signed=False)
        if self._frameis_ok(packet, rth_length, payload_length):
            if self.driver == DRIVER_RALINK:
                self.signal = packet[14]
                # self.datarate = packet[9]
            else:
                self.signal = packet[30]
            payload_start = rth_length + DB_80211_HEADER_LENGTH
            return packet[payload_start:(payload_start+payload_length)]
        else:
            return False

    def _frameis_ok(self, packet, radiotap_header_length, payload_length):
        # TODO: check crc8 of header or something; currently: dump unusual large frames (ones much larger than payload)
        if (radiotap_header_length + payload_length + DB_80211_HEADER_LENGTH + 20)>len(packet):
            return True
        else:
            print(self.tag+ "Found a DroneBridge Frame that is not OK - ignoring it")
            return False

    def _process_smartphonecommand(self, raw_data, thelast_keepalive):
        try:
            raw_data_decoded = bytes.decode(raw_data)
            print("Received from SP: " + raw_data_decoded)
            if raw_data == "smartphone_is_still_here":
                return time.time()
        except UnicodeDecodeError as unicodeerr:
            pass
        if not self._route_db_comm_protocol(raw_data):
            print(self.tag + "smartphone command could not be processed correctly")
        return thelast_keepalive

    def _route_db_comm_protocol(self, raw_data_encoded):
        status = False
        extracted_info = comm_message_extract_info(raw_data_encoded) # returns json bytes and crc bytes
        loaded_json = json.loads(extracted_info[0].decode())

        if loaded_json['destination'] == 1 and self.comm_direction == TO_DRONE and check_package_good(extracted_info):
            message = self._process_db_comm_protocol_type(loaded_json)
            if message != "":
                status = self.sendto_smartphone(message, self.COMM_PORT_SMARTPHONE)
            else:
                status = True
        elif loaded_json['destination'] == 2 and check_package_good(extracted_info):
            if self.comm_direction == TO_DRONE:
                response_drone = self._redirect_comm_to_drone(raw_data_encoded)
                if response_drone != False and response_drone!=None:
                    message = self._process_db_comm_protocol_type(loaded_json)
                    self.sendto_smartphone(message, self.COMM_PORT_SMARTPHONE)
                    status = self.sendto_smartphone(response_drone, self.COMM_PORT_SMARTPHONE)
            else:
                message = self._process_db_comm_protocol_type(loaded_json)
                sentbytes = self.sendto_groundstation(message, PORT_COMMUNICATION)
                if sentbytes == None:
                    status = True
        elif loaded_json['destination'] == 3:
            if self.comm_direction == TO_DRONE:
                status = self._sendto_drone(raw_data_encoded, PORT_COMMUNICATION)
            else:
                change_settings_gopro(loaded_json)
        elif loaded_json['destination'] == 4:
            if self.comm_direction == TO_DRONE:
                status = self.sendto_smartphone(raw_data_encoded, self.COMM_PORT_SMARTPHONE)
        else:
            print(self.tag + "DB_COMM_PROTO: Unknown message destination")
        return status

    def _process_db_comm_protocol_type(self, loaded_json):
        message = ""
        if loaded_json['type'] == 'mspcommand':
            self._sendto_drone(base64.b64decode(loaded_json['MSP']), PORT_CONTROLLER)
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
        """This one will send to drone till it receives a valid response. Response is returned or False"""
        if self.first_run:
            self._clear_monitor_comm_socket_buffer()
            self.first_run = False
        self._sendto_drone(raw_data_encoded, PORT_COMMUNICATION)
        print("Forwarding to drone:")
        print(raw_data_encoded)
        response = self.receive_datafromdrone()
        print("Parsed packet received from drone:")
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
        depending on message type different ports/programmes aka frontends on the drone need to be addressed
        """
        if port_bytes == PORT_CONTROLLER:
            print(self.tag + "Sending MSP command to RX Controller (wifi)")
            try:
                raw_socket = socket(AF_PACKET, SOCK_RAW)
                raw_socket.bind((self.interface, 0))
                num = raw_socket.send(self.dst_mac + self.src_mac + ETH_TYPE + raw_data_bytes)
                raw_socket.close()
            except Exception as e:
                print(self.tag + str(e) + ": Are you sure this program was run as superuser?")
                return False
            print(self.tag + "Sent it! " + str(num))
        else:
            print(self.tag + "Sending a message to telemetry frontend on drone")
            num = self.comm_sock.sendto(raw_data_bytes, (self.ip_rx, self.udp_port_rx))
        return num

    def _send_monitor(self, data_bytes, port_bytes, direction):
        """Send a packet in monitor mode"""
        payload_length_bytes = bytes(len(data_bytes).to_bytes(2, byteorder='little', signed=False))
        crc_content = bytes(bytearray(DB_FRAME_VERSION + port_bytes + direction + payload_length_bytes))
        crc = crc8.crc8()
        crc.update(crc_content)
        ieee_min_header_mod = bytes(
            bytearray(self.fcf + b'\x00\x00' + self.comm_id + self.src_mac + crc_content + crc.digest() + b'\x00\x00'))
        while True:
            r, w, e = select.select([], [self.comm_sock], [], 0)
            if w:
                num = self.comm_sock.sendall(RADIOTAP_HEADER + ieee_min_header_mod + data_bytes)
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
            raw_socket = attach_filter(raw_socket, TO_DRONE, self.comm_id, self.db_port)  # filter for packets TO_DRONE
        else:
            raw_socket = attach_filter(raw_socket, TO_GROUND, self.comm_id, self.db_port)  # filter for packets TO_GROUND
        return raw_socket

    def _set_comm_socket_behavior(self, thesocket):
        """Set to blocking or non-blocking depending on Module (Telemetry, Communication) and if on drone or ground"""
        adjusted_socket = thesocket
        if self.comm_direction == TO_GROUND and self.db_port == PORT_TELEMETRY:  # On drone side in telemetry module
            adjusted_socket.setblocking(False)
        elif self.comm_direction == TO_DRONE and self.db_port == PORT_COMMUNICATION:  # On ground side in comm module
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
        print(self.tag + "Opening UDP-Socket to smartphone on port: "+str(self.udp_port_smartphone)+")")
        sock = socket(AF_INET, SOCK_DGRAM)
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
        address = ('', self.udp_port_smartphone)
        sock.bind(address)
        sock.setblocking(False)
        print("Done")
        return sock