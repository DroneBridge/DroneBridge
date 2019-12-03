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

from enum import Enum
from select import select
from socket import socket, AF_PACKET, SOCK_RAW, htons, timeout
from syslog import LOG_WARNING

from Cryptodome.Cipher import AES

from bpf import attach_filter
from db_helpers import db_log


class DBPort(Enum):
    DB_PORT_CONTROLLER = b'\x01'
    DB_PORT_TELEMETRY = b'\x02'
    DB_PORT_VIDEO = b'\x03'
    DB_PORT_COMMUNICATION = b'\x04'
    DB_PORT_STATUS = b'\x05'
    DB_PORT_PROXY = b'\x06'
    DB_PORT_RC = b'\x07'
    DB_PORT_GENERIC_1 = b'\x08'
    DB_PORT_GENERIC_2 = b'\x09'
    DB_PORT_GENERIC_3 = b'\x0a'
    DB_PORT_GENERIC_4 = b'\x0b'
    DB_PORT_GENERIC_5 = b'\x0c'


class DBDir(Enum):
    DB_TO_UAV = b'\x01'
    DB_TO_GND = b'\x03'

    @property
    def int_val(self) -> int:
        return int.from_bytes(self.value, byteorder="little")


class DBMode(Enum):
    MONITOR = 'm'
    WIFI = 'w'


DB_RAW_OFFSET_BYTES = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
DB_RAW_OFFSET = len(DB_RAW_OFFSET_BYTES)
DB_HEADER_LENGTH = 10
DB_RAW_ENCRYPT_NONCE_LENGTH = 16
DB_RAW_ENCRYPT_MAC_LENGTH = 16
DB_RAW_ENCRYPT_HEADER_LENGTH = DB_RAW_ENCRYPT_NONCE_LENGTH + DB_RAW_ENCRYPT_MAC_LENGTH


class DroneBridge:
    """
    Handles all data transmission using the DroneBridgeLib raw protocol. Creates sockets and supplies methods for sending
    and receiving messages. Support for diversity transmission and receiving. Support for AES128, AES192 & AES256
    """

    # when adhering the 802.11 header the payload is offset to not be overwritten by SQN. (Use with non patched drivers)
    list_lr_sockets = []
    MONITOR_BUFFERSIZE_COMM = 2048

    def __init__(self, send_direction: DBDir, interfaces: list, mode: DBMode, communication_id: int or bytes,
                 dronebridge_port: DBPort or bytes, tag="MyDBApplication", db_blocking_socket=True, frame_type=1,
                 transmission_bitrate=36, compatibility_mode=False, encryption_key=None):
        """
        Class that handles communication between multiple wifi cards in monitor mode using the DroneBridgeLib raw protocol

        :param send_direction: Direction in that the packets will be sent (to UAV or to ground station)
        :param interfaces: List of wifi interfaces in monitor mode used to send & receive data
        :param mode: DroneBridgeLib operating mode. Only 'm' for monitor supported for now
        :param communication_id: [0-255] to identify a communication link. Must be same on all communication partners
        :param dronebridge_port: DroneBridgeLib port to listen for incoming packets
        :param tag: Name db_loged in front of every log message
        :param db_blocking_socket: Should the opened sockets block on receiving
        :param frame_type: [1|2] for RTS|DATA. 80211 frame type used to send message. Data & RTS frames supported
        :param transmission_bitrate: Only supported by some Ralink cards. Set packet specific transmission rate
        :param compatibility_mode: Adheres the 80211 packet standard by not writing payload data into the header.
            Enable if you want to communicate with partners that do not have patched drivers etc. -> longer packets
        :param encryption_key: If supplied all raw messages will be encrypted using AES. Must be a String representing
            the HEX bytes of the secret key. Must have a length of 32, 48 or 64 characters representing 128bit, 192bit
            and 256bit AES encryption. Eg. "3373367639792442264528482B4D6251" for 128bit encryption
        """
        assert type(send_direction) is DBDir
        assert type(mode) is DBMode
        assert type(dronebridge_port) is (DBPort or bytes)
        assert type(transmission_bitrate) is int
        self.mode = mode
        self.tag = tag
        self.send_direction = send_direction
        if self.send_direction == DBDir.DB_TO_GND:
            self.recv_direction = DBDir.DB_TO_UAV
        else:
            self.recv_direction = DBDir.DB_TO_GND
        self.frame_type = frame_type
        if type(dronebridge_port) is bytes:
            self.db_port = dronebridge_port
        elif type(dronebridge_port) is DBPort:
            self.db_port = dronebridge_port.value
        self.adhere_80211_header = compatibility_mode
        if type(communication_id) is int:
            self.comm_id = bytes([communication_id])  # must be the same on drone and ground station
        else:
            self.comm_id = communication_id  # must be the same on drone and ground station
        self._seq_num = 0  # for transmitting
        self.recv_seq_num = 0  # contains sequence number of last packet received via receive_data()
        for _interface in interfaces:
            self.list_lr_sockets.append(self._open_comm_sock(_interface, blocking_socket=db_blocking_socket))
        if frame_type == 1:
            self.fcf = b'\xb4\x00\x00\x00'  # RTS frame
        else:
            self.fcf = b'\x08\x00\x00\x00'  # Data frame
        self.rth = self.generate_radiotap_header(6)
        self.set_transmission_bitrate(transmission_bitrate)
        self.use_encryption = False
        if encryption_key is not None and len(encryption_key) == (32 or 48 or 64):
            if isinstance(encryption_key, bytes):
                self.aes_key = encryption_key
                self.use_encryption = True
            elif isinstance(encryption_key, str):
                self.aes_key = bytes.fromhex(encryption_key)
                self.use_encryption = True

    def sendto_ground_station(self, data_bytes: bytes, db_port: DBPort):
        """Convenient function. Send stuff to the ground station"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridgeLib")
        else:
            self.send_monitor(data_bytes, db_port.value, DBDir.DB_TO_GND.value)

    def sendto_uav(self, data_bytes: bytes, db_port: DBPort):
        """Convenient function. Send stuff to the UAV"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridgeLib")
        else:
            self.send_monitor(data_bytes, db_port.value, DBDir.DB_TO_UAV.value)

    def send_monitor(self, data_bytes: bytes, port_bytes: bytes, direction: bytes):
        """
        Send a packet in monitor mode using DroneBridgeLib raw protocol v2

        :param data_bytes: Payload
        :param port_bytes: DroneBridgeLib raw protocol destination port
        :param direction: DroneBridgeLib raw protocol direction
        """
        if self.use_encryption:
            cypher = AES.new(self.aes_key, AES.MODE_EAX)
            text, tag = cypher.encrypt_and_digest(data_bytes)
            data_bytes = cypher.nonce + tag + text
        payload_length = len(data_bytes)
        if payload_length >= 1480:
            db_log(f"{self.tag}: WARNING - Payload might be too big for a single transmission! {payload_length}>=1480")
        elif (payload_length < 6 and self.frame_type == 1) or (payload_length < 14 and self.frame_type > 1):
            db_log(f"{self.tag}: ERROR - Payload length too small for specified frame type (min. payload length -> "
                   f"RTS: 6 bytes, DATA/BEACON: 14 bytes)")
        if self._seq_num == 255:
            self._seq_num = 0
        else:
            self._seq_num += 1
        db_v2_raw_header = bytes(bytearray(self.fcf + direction + self.comm_id + port_bytes +
                                           bytes(payload_length.to_bytes(2, byteorder='little', signed=False)) +
                                           bytes([self._seq_num])))
        if self.adhere_80211_header:
            raw_buffer = self.rth + db_v2_raw_header + DB_RAW_OFFSET_BYTES + data_bytes
        else:
            raw_buffer = self.rth + db_v2_raw_header + data_bytes

        _, writeable, _ = select([], self.list_lr_sockets, [])  # send on all free cards but at least on one of them
        for writeable_sock in writeable:
            writeable_sock.sendall(raw_buffer)

    def receive_data(self, receive_timeout=None) -> bytes:
        """
        Select on all long range sockets and receive packet with diversity

        :param receive_timeout: Max time [s] to wait for a packet. None for blocking. Returns empty bytes on timeout
        :return: False on timeout, packet payload on success
        """
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridgeLib")
        else:
            try:
                payload = b''
                readable, _, _ = select(self.list_lr_sockets, [], [], receive_timeout)
                for readable_socket in readable:  # receive on all sockets to clear buffers
                    data, seq_num = self.parse_packet(bytearray(readable_socket.recv(self.MONITOR_BUFFERSIZE_COMM)))
                    if seq_num != self.recv_seq_num:  # Only return data that was not returned before (diversity)
                        payload = data
                        self.recv_seq_num = seq_num
                return payload
            except timeout as t:
                db_log(f"{self.tag}: Socket timed out. No response received from drone (monitor mode) -> {t}")
                return b''
            except Exception as e:
                db_log(f"{self.tag}: Error receiving data form drone (monitor mode) -> {e}")
                return b''

    def set_transmission_bitrate(self, new_bitrate: int):
        """
        Only supported with Ralink chipsets! In any other case the transmission rate is set during monitor mode init

        :param new_bitrate: [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]
        """
        if new_bitrate in [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]:
            self.rth = self.generate_radiotap_header(new_bitrate)
        else:
            db_log(f"{self.tag}: Selected bitrate {new_bitrate} not supported [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]")

    def clear_socket_buffers(self):
        """Read all bytes available from the sockets and send the received data to nirvana"""
        readable, _, _ = select(self.list_lr_sockets, [], [], 1)
        for read_sock in readable:
            read_sock.recv(8192)

    def close_sockets(self):
        """Close all DroneBridgeLib raw sockets"""
        for sock in self.list_lr_sockets:
            sock.close()

    def parse_packet(self, packet: bytes) -> (bytes, int):
        """
        Parse DroneBridgeLib raw protocol v2. Returns packet payload and sequence number. Decrypts packet if necessary

        :param packet: Bytes of a received packet via monitor mode including radiotap header
        :return: Tuple: packet payload as bytes, packet sequence number
        """
        # packet[2]: Length of radiotap header
        if packet[packet[2] + 4] != self.recv_direction.int_val:
            db_log(f"{self.tag}: Parser - Packet not addressed to us (Receive direction: {self.recv_direction} "
                   f"{self.recv_direction.int_val}, Packet direction: {packet[packet[2] + 4]}). Ignoring", LOG_WARNING)
            return b'', int(packet[packet[2] + 9])
        db_v2_payload_length = int.from_bytes(packet[(packet[2] + 7):(packet[2] + 8)] +
                                              packet[(packet[2] + 8):(packet[2] + 9)],
                                              byteorder='little', signed=False)
        if (len(packet) - packet[2] - DB_HEADER_LENGTH) <= (db_v2_payload_length + 4):
            payload_start = packet[2] + DB_HEADER_LENGTH
        else:
            payload_start = packet[2] + DB_HEADER_LENGTH + DB_RAW_OFFSET  # for adhere_80211_header packets
        if self.use_encryption:
            nonce = packet[payload_start:(payload_start + DB_RAW_ENCRYPT_NONCE_LENGTH)]
            tag = packet[(payload_start + DB_RAW_ENCRYPT_NONCE_LENGTH):(
                    payload_start + DB_RAW_ENCRYPT_NONCE_LENGTH + DB_RAW_ENCRYPT_MAC_LENGTH)]
            cypher = AES.new(self.aes_key, AES.MODE_EAX, nonce)
            data_bytes = cypher.decrypt(
                packet[(payload_start + DB_RAW_ENCRYPT_HEADER_LENGTH):(payload_start + db_v2_payload_length)])
            try:
                cypher.verify(tag)
                return data_bytes, int(packet[packet[2] + 9])
            except ValueError:
                db_log(f"{self.tag}: ERROR - Can not decrypt payload. Key incorrect or message corrupt")
            return b'', int(packet[packet[2] + 9])
        else:
            return packet[payload_start:(payload_start + db_v2_payload_length)], int(packet[packet[2] + 9])

    @staticmethod
    def generate_radiotap_header(rate: int) -> bytes:
        """
        Generate a valid radiotap header with defined bit rate

        :param rate: Transmission bit rate: 1, 2, 6, 9, 12, 18, 24, 36, 48, 54
        :return: Bytes representing the radiotap header
        """
        return b'\x00\x00\x0c\x00\x04\x80\x00\x00' + bytes([rate * 2]) + b'\x00\x18\x00'

    def _open_comm_sock(self, network_interface: str, blocking_socket=True) -> socket:
        """Opens a socket that uses monitor mode and DroneBridgeLib raw protocol"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridgeLib")
        else:
            return self._open_monitor_socket(network_interface, blocking=blocking_socket)

    def _open_monitor_socket(self, network_interface: str, blocking=True) -> socket:
        """
        Opens a socket on an interface set into monitor mode. Applies a Bercley Packet Filter to the socket so that only
        DroneBridgeLib raw protocol packets can be received.

        :param network_interface: Name of the network interface to bind the socket to
        :param blocking: Blocking behavior of the socket
        :return: The socket file descriptor
        """
        raw_socket = socket(AF_PACKET, SOCK_RAW, htons(0x0004))
        raw_socket.bind((network_interface, 0))
        raw_socket.setblocking(blocking)
        assert (isinstance(self.recv_direction.value, bytes))
        raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=self.recv_direction.value,
                                   byte_port=self.db_port)
        return raw_socket
