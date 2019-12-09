#
#   This file is part of DroneBridgeLib: https://github.com/seeul8er/DroneBridge
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

import time

from DroneBridge import DroneBridge, DBPort, DBDir, DBMode

list_wifi_interfaces = ['wlx00c0ca973410']  # must be in monitor mode
communication_id = 22  # idents a link, multiple systems with same ports on same frequency possible
send_recv_port = DBPort.DB_PORT_GENERIC_1  # Virtual port to which the packet is addressed to
frame_type = 1  # RTS, 2 DATA
compatibility_mode = False  # Enable with unpatched Kernels etc. (try without first)
send_direction = DBDir.DB_TO_UAV  # Direction of the packet. Reverse direction on receiving side needed

# Set to None for unencrypted link. Must be the same on receiving side.
# length of 32, 48 or 64 characters representing 128bit, 192bit and 256bit AES encryption
encrypt_key = "3373367639792442264528482B4D6251"  # bytes or string representing HEX

payload_data = b'HelloEveryone.IamPayload!LifeIsEasyWhenYouArePayload'
dronebridge = DroneBridge(send_direction, list_wifi_interfaces, DBMode.MONITOR, communication_id, send_recv_port,
                          tag="Test_Sender", db_blocking_socket=True, frame_type=frame_type,
                          compatibility_mode=compatibility_mode, encryption_key=encrypt_key)

list_sockets_raw = dronebridge.list_lr_sockets  # List of raw sockets to be used for manual send/receive operations

for i in range(10000):
    time.sleep(0.5)
    dronebridge.sendto_uav(payload_data, send_recv_port)
    print("\r" + str(i), end='')

print("\nDone sending!")
