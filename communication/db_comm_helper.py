# This file is part of DroneBridge licenced under Apache Licence 2
# https://github.com/seeul8er/DroneBridge/
# Created by Wolfgang Christl

def find_mac(interface):
    f = open("/sys/class/net/"+interface+"/address", 'r',)
    mac_bytes = bytes(bytearray.fromhex(f.read(17).replace(':', '')))
    f.close()
    return mac_bytes

