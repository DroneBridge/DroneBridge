#
#   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
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

from DroneBridge import DroneBridge, DBDir, DBMode, DBPort

list_wifi_interfaces = ['wlx8416f916382c']  # must be in monitor mode
communication_id = 22
send_recv_port = DBPort.DB_PORT_GENERIC_1
frame_type = 1  # RTS, 2 DATA
compatibility_mode = False
send_direction = DBDir.DB_TO_GND

# Set to None for unencrypted link. Must be the same on receiving side.
# length of 32, 48 or 64 characters representing 128bit, 192bit and 256bit AES encryption
encrypt_key = "3373367639792442264528482B4D6251"  # bytes or string representing HEX

dronebridge = DroneBridge(send_direction, list_wifi_interfaces, DBMode.MONITOR, communication_id, send_recv_port,
                          tag="Test_Receiver", db_blocking_socket=True, frame_type=frame_type,
                          compatibility_mode=compatibility_mode, encryption_key=encrypt_key)

dronebridge.clear_socket_buffers()

for i in range(100):
    received_payload = dronebridge.receive_data()
    if received_payload:
        print("Received: " + received_payload.decode())
