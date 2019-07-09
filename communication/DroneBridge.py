#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
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

from bpf import attach_filter


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


class DroneBridge:
    """
    Handles all data transmission using the DroneBridge raw protocol. Creates sockets and supplies methods for sending
    and receiving messages. Support for diversity transmission and receiving
    """

    # when adhering the 802.11 header the payload is offset to not be overwritten by SQN. (Use with non patched drivers)
    list_lr_sockets = []
    MONITOR_BUFFERSIZE_COMM = 2048

    def __init__(self, comm_direction: DBDir, interfaces: list, mode: DBMode, communication_id: int or bytes,
                 dronebridge_port: DBPort or bytes, tag="MyDBApplication", db_blocking_socket=True, frame_type="rts",
                 transmission_bitrate=36, compatibility_mode=False):
        """
        Class that handles communication between multiple wifi cards in monitor mode using the DroneBridge raw protocol

        :param comm_direction: Direction in that the packets will be sent (to UAV or to ground station)
        :param interfaces: List of wifi interfaces in monitor mode used to send & receive data
        :param mode: DroneBridge operating mode. Only 'm' for monitor supported for now
        :param communication_id: [0-255] to identify a communication link. Must be same on all communication partners
        :param dronebridge_port: DroneBridge port to listen for incoming packets
        :param tag: Name printed in front of every log message
        :param db_blocking_socket: Should the opened sockets block on receiving
        :param frame_type: [rts|data] 80211 frame type used to send message. Data & RTS frames supported
        :param transmission_bitrate: Only supported by some Ralink cards. Set packet specific transmission rate
        :param compatibility_mode: Adheres the 80211 packet standard by not writing payload data into the header.
            Enable if you want to communicate with partners that do not have patched drivers etc. -> longer packets
        """
        assert type(comm_direction) is DBDir
        assert type(mode) is DBMode
        assert type(dronebridge_port) is (DBPort or bytes)
        assert type(transmission_bitrate) is int
        self.mode = mode
        self.tag = tag
        self.comm_direction = comm_direction
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
        if frame_type == "rts":
            self.fcf = b'\xb4\x00\x00\x00'  # RTS frame
        else:
            self.fcf = b'\x08\x00\x00\x00'  # Data frame
        self.rth = self.generate_radiotap_header(6)
        self.set_transmission_bitrate(transmission_bitrate)

    def sendto_ground_station(self, data_bytes: bytes, db_port: DBPort):
        """Convenient function. Send stuff to the ground station"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridge")
        else:
            self.send_monitor(data_bytes, db_port.value, DBDir.DB_TO_GND.value)

    def sendto_uav(self, data_bytes: bytes, db_port: DBPort):
        """Convenient function. Send stuff to the UAV"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridge")
        else:
            self.send_monitor(data_bytes, db_port.value, DBDir.DB_TO_UAV.value)

    def send_monitor(self, data_bytes: bytes, port_bytes: bytes, direction: bytes):
        """
        Send a packet in monitor mode using DroneBridge raw protocol v2

        :param data_bytes: Payload
        :param port_bytes: DroneBridge raw protocol destination port
        :param direction: DroneBridge raw protocol direction
        """
        if len(data_bytes) >= 1480:
            print(f"{self.tag}: WARNING - Payload might be too big for a single transmission! {len(data_bytes)}>=1480")
        payload_length_bytes = bytes(len(data_bytes).to_bytes(2, byteorder='little', signed=False))
        if self._seq_num == 255:
            self._seq_num = 0
        else:
            self._seq_num += 1
        db_v2_raw_header = bytes(bytearray(self.fcf + direction + self.comm_id + port_bytes + payload_length_bytes +
                                           bytes([self._seq_num])))
        if self.adhere_80211_header:
            raw_buffer = self.rth + db_v2_raw_header + DB_RAW_OFFSET_BYTES + data_bytes
        else:
            raw_buffer = self.rth + db_v2_raw_header + data_bytes

        _, writeable, _ = select([], self.list_lr_sockets, [], 0)  # send on all free cards but at least on one of them
        for writeable_sock in writeable:
            writeable_sock.sendall(raw_buffer)

    def receive_data(self, receive_timeout=1.5) -> bytes or False:
        """
        Select on all long range sockets and receive packet with diversity

        :param receive_timeout: Max time [s] to wait for a packet. Returns False on timeout
        :return: False on timeout, packet payload on success
        """
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridge")
        else:
            try:
                payload = False
                readable, _, _ = select(self.list_lr_sockets, [], [], receive_timeout)
                for readable_socket in readable:  # receive on all sockets to clear buffers
                    data, seq_num = self.parse_packet(bytearray(readable_socket.recv(self.MONITOR_BUFFERSIZE_COMM)))
                    if seq_num != self.recv_seq_num:  # Only return data that was not returned before (diversity)
                        payload = data
                        self.recv_seq_num = seq_num
                return payload
            except timeout as t:
                print(f"{self.tag}: Socket timed out. No response received from drone (monitor mode) -> {t}")
                return False
            except Exception as e:
                print(f"{self.tag}: Error receiving data form drone (monitor mode) -> {e}")
                return False

    def set_transmission_bitrate(self, new_bitrate: int):
        """
        Only supported with Ralink chipsets! In any other case the transmission rate is set during monitor mode init

        :param new_bitrate: [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]
        """
        if new_bitrate in [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]:
            self.rth = self.generate_radiotap_header(new_bitrate)
        else:
            print(f"{self.tag}: Selected bitrate {new_bitrate} not supported [1, 2, 6, 9, 12, 18, 24, 36, 48, 54]")

    def clear_socket_buffers(self):
        """Read all bytes available from the sockets and send the received data to nirvana"""
        readable, _, _ = select(self.list_lr_sockets, [], [], 1)
        for read_sock in readable:
            read_sock.recv(8192)

    def close_sockets(self):
        """Close all DroneBridge raw sockets"""
        for sock in self.list_lr_sockets:
            sock.close()

    @staticmethod
    def parse_packet(packet: bytes) -> (bytes, int):
        """
        Parse DroneBridge raw protocol v2. Returns packet payload and sequence number

        :param packet: Bytes of a received packet via monitor mode including radiotap header
        :return: Tuple: packet payload as bytes, packet sequence number
        """
        # packet[2]: Length of radiotap header
        db_v2_payload_length = int.from_bytes(packet[(packet[2] + 7):(packet[2] + 8)] +
                                              packet[(packet[2] + 8):(packet[2] + 9)],
                                              byteorder='little', signed=False)
        if (len(packet) - packet[2] - DB_HEADER_LENGTH) <= (db_v2_payload_length + 4):
            payload_start = packet[2] + DB_HEADER_LENGTH
        else:
            payload_start = packet[2] + DB_HEADER_LENGTH + DB_RAW_OFFSET  # for adhere_80211_header packets
        return packet[payload_start:(payload_start + db_v2_payload_length)], int(packet[packet[2] + 10])

    @staticmethod
    def generate_radiotap_header(rate: int) -> bytes:
        """
        Generate a valid radiotap header with defined bit rate

        :param rate: Transmission bit rate: 1, 2, 6, 9, 12, 18, 24, 36, 48, 54
        :return: Bytes representing the radiotap header
        """
        return b'\x00\x00\x0c\x00\x04\x80\x00\x00' + bytes([rate * 2]) + b'\x00\x18\x00'

    def _open_comm_sock(self, network_interface: str, blocking_socket=True) -> socket:
        """Opens a socket that uses monitor mode and DroneBridge raw protocol"""
        if self.mode is DBMode.WIFI:
            raise NotImplementedError("Wifi mode is currently not supported by DroneBridge")
        else:
            return self._open_monitor_socket(network_interface, blocking=blocking_socket)

    def _open_monitor_socket(self, network_interface: str, blocking=True) -> socket:
        """
        Opens a socket on an interface set into monitor mode. Applies a Bercley Packet Filter to the socket so that only
        DroneBridge raw protocol packets can be received.

        :param network_interface: Name of the network interface to bind the socket to
        :param blocking: Blocking behavior of the socket
        :return: The socket file descriptor
        """
        raw_socket = socket(AF_PACKET, SOCK_RAW, htons(0x0004))
        raw_socket.bind((network_interface, 0))
        raw_socket.setblocking(blocking)
        if self.comm_direction == DBDir.DB_TO_GND:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=DBDir.DB_TO_UAV.value,
                                       byte_port=self.db_port)  # filter for packets TO_DRONE
        else:
            raw_socket = attach_filter(raw_socket, byte_comm_id=self.comm_id, byte_direction=DBDir.DB_TO_GND.value,
                                       byte_port=self.db_port)  # filter for packets TO_GROUND
        return raw_socket
