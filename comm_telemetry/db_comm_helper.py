def find_mac(interface):
    f = open("/sys/class/net/"+interface+"/address", 'r',)
    mac_bytes = bytes(bytearray.fromhex(f.read(17).replace(':', '')))
    f.close()
    return mac_bytes

