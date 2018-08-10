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

import time
import sysv_ipc
import netifaces

# memory map
# 111111 = smartphone ip
key_smartphone_ip_sm = 1111
key_smartphone_ip_sem = 1112


class DB_IP_GETTER():

    def __init__(self):
        try:
            self.sem = sysv_ipc.Semaphore(key_smartphone_ip_sem, sysv_ipc.IPC_CREX)
        except sysv_ipc.ExistentialError:
            # One of my peers created the semaphore already
            self.sem = sysv_ipc.Semaphore(key_smartphone_ip_sem)
            # Waiting for that peer to do the first acquire or release
            while not self.sem.o_time:
                time.sleep(.1)
        else:
            # Initializing sem.o_time to nonzero value
            self.sem.release()
        try:
            self.memory = sysv_ipc.SharedMemory(key_smartphone_ip_sm, sysv_ipc.IPC_CREX)
        except sysv_ipc.ExistentialError:
            self.memory = sysv_ipc.SharedMemory(key_smartphone_ip_sm)

    def return_smartphone_ip(self):
        # acquire causes lag. Unsolved for the moment. Might be because of status module
        # self.sem.acquire()
        ip = str(self.memory.read(key_smartphone_ip_sm).strip(), 'utf-8')
        # self.sem.release()
        return ip


def find_smartphone_ip():
    pi_interfaces = netifaces.interfaces()
    if 'usb0' in pi_interfaces:
        try:
            g = netifaces.gateways()
            for interface_desc in g[netifaces.AF_INET]:
                if interface_desc[1] == 'usb0':
                    return interface_desc[0] + '    '
            return "192.168.2.2    "
        except KeyError:
            print("IP_CHECKER: KeyError")
            return "192.168.42.129 "
    else:
        return "192.168.2.2    "


def main():
    print("DB_IPCHECKER: starting")
    try:
        memory = sysv_ipc.SharedMemory(key_smartphone_ip_sm, sysv_ipc.IPC_CREX)
    except sysv_ipc.ExistentialError:
        memory = sysv_ipc.SharedMemory(key_smartphone_ip_sm)
    try:
        sem = sysv_ipc.Semaphore(key_smartphone_ip_sem, sysv_ipc.IPC_CREX)
    except sysv_ipc.ExistentialError:
        # One of my peers created the semaphore already
        sem = sysv_ipc.Semaphore(key_smartphone_ip_sem)
        # Waiting for that peer to do the first acquire or release
        while not sem.o_time:
            time.sleep(.1)
    else:
        # Initializing sem.o_time to nonzero value
        sem.release()

    while (True):
        time.sleep(2)
        sem.acquire()
        memory.write(find_smartphone_ip())
        sem.release()


if __name__ == "__main__":
    main()
