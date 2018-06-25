#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2017 Wolfgang Christl
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

import ctypes
import mmap
import time

from shmemctypes import ShmemRawArray


class wifi_adapter_rx_status_t(ctypes.Structure):
    _fields_ = [
        ('received_packet_cnt', ctypes.c_uint32),
        ('wrong_crc_cnt', ctypes.c_uint32),
        ('current_signal_dbm', ctypes.c_int8),
        ('type', ctypes.c_int8)
    ]


class WBC_RX_Status(ctypes.Structure):
    _fields_ = [
        ('last_update', ctypes.c_int32),
        ('received_block_cnt', ctypes.c_uint32),
        ('damaged_block_cnt', ctypes.c_uint32),
        ('lost_packet_cnt', ctypes.c_uint32),
        ('received_packet_cnt', ctypes.c_uint32),
        ('tx_restart_cnt', ctypes.c_uint32),
        ('kbitrate', ctypes.c_uint32),
        ('wifi_adapter_cnt', ctypes.c_uint32),
        ('adapter', wifi_adapter_rx_status_t * 8)
    ]


def open_shm():
    f = open("/wifibroadcast_rx_status_0", "r+b")
    return mmap.mmap(f.fileno(), 0)

def read_wbc_status(mapped_structure):
    wbc_status = WBC_RX_Status.from_buffer(mapped_structure)
    print(str(wbc_status.kbitrate)+"kbit/s"+" "+str(wbc_status.damaged_block_cnt)+" damages blocks")


def main():
    print("DB_WBC_STATUSREADER: starting")
    shared_data = ShmemRawArray(WBC_RX_Status, 0, "/wifibroadcast_rx_status_0", False)
    #mymap = open_shm()
    while(True):
        for d in shared_data:
            print(str(d.received_block_cnt))
        time.sleep(1)


if __name__ == "__main__":
    main()