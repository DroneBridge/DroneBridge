import argparse
import time
from subprocess import Popen
from DroneBridge_Protocol import DBProtocol
from db_comm_helper import find_mac

# Default values, may get overridden by command line arguments

UDP_Port_TX = 1604  # Port for communication with TX (Groundstation)
IP_TX = "192.168.3.2"   # Target IP address (IP address of the Groundstation - not important and gets overridden anyways)
UDP_buffersize = 512  # bytes
AB_INTERFACE = "wlan1"
dst = b''

def getGoPro_Status_JSON():
    return b"GoPro-Test Status"
    #return requests.get('http://10.5.5.9/gp/gpControl/status').json()


def setupVideo(mode):
    if mode == "wifi":
        proc = Popen("pp_rx_keepgopro.py", shell=True, stdin=None, stdout=None, stderr=None, close_fds=True)


def parseArguments():
    parser = argparse.ArgumentParser(description='Put this file on RX (drone). It handles telemetry and GoPro settings.')
    parser.add_argument('-i', action='store', dest='DB_INTERFACE',
                        help='Network interface on which we send out packets to drone. Should be interface '
                             'for long range comm (default: wlan1)',
                        default='wlan1')
    parser.add_argument('-p', action="store", dest='udp_port_tx', help='Local and remote port on which we need to address '
                                                                       'our packets for the groundstation and listen for '
                                                                       'commands coming from groundstation (same port '
                                                                       'number on TX and RX - you may not change'
                                                                       ' default: 1604)', type=int, default=1604)
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-a', action='store', dest='frame_type',
                        help='Specify frame type. Use <1> for Ralink chips (data frame) and <2> for Atheros chips '
                             '(beacon frame). No CTS supported. Options [1|2]', default='1')
    parser.add_argument('-c', action='store', dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. 8 characters long. Allowed '
                             'chars are (0123456789abcdef) Example: "aabb0011"', default='aabbccdd')
    return parser.parse_args()


def main():
    global SerialPort, UDP_Port_TX, IP_TX
    parsedArgs = parseArguments()
    UDP_Port_TX = parsedArgs.udp_port_tx
    mode = parsedArgs.mode
    frame_type = parsedArgs.frame_type
    DB_INTERFACE = parsedArgs.DB_INTERFACE
    src = find_mac(DB_INTERFACE)
    extended_comm_id = bytes(b'\x01'+b'\x02'+bytearray.fromhex(parsedArgs.comm_id)) # <odd><direction><comm_id>
    # print("DB_TX_Comm: Communication ID: " + comm_id.hex()) # only works in python 3.5+
    print("DB_RX_Comm: Communication ID: " + str(extended_comm_id))
    dbprotocol = DBProtocol(src, dst, UDP_Port_TX, IP_TX, 0, b'\x02', DB_INTERFACE, mode, extended_comm_id, frame_type, b'\x04')

    #setupVideo(mode)
    #time.sleep(2)
    while True:
        dbprotocol.receive_process_datafromgroundstation() # blocking


if __name__ == "__main__":
    main()
