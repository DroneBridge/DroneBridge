import argparse
from subprocess import Popen
from DroneBridge_Protocol import DBProtocol
from db_comm_helper import find_mac


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
    parser.add_argument('-n', action='store', dest='DB_INTERFACE',
                        help='Network interface on which we send out packets to drone. Should be interface '
                             'for long range comm (default: wlan1)',
                        default='wlan1')
    parser.add_argument('-m', action='store', dest='mode',
                        help='Set the mode in which communication should happen. Use [wifi|monitor]',
                        default='monitor')
    parser.add_argument('-a', action='store', dest='frame_type',
                        help='Specify frame type. Options [1|2]', default='1')
    parser.add_argument('-c', action='store', type=int, dest='comm_id',
                        help='Communication ID must be the same on drone and groundstation. A number between 0-255 '
                             'Example: "125"', default='111')
    return parser.parse_args()


def main():
    global SerialPort, UDP_Port_TX, IP_TX
    parsedArgs = parseArguments()
    UDP_Port_TX = 1604
    mode = parsedArgs.mode
    frame_type = parsedArgs.frame_type
    DB_INTERFACE = parsedArgs.DB_INTERFACE
    src = find_mac(DB_INTERFACE)
    comm_id = bytes([parsedArgs.comm_id])
    # print("DB_TX_Comm: Communication ID: " + comm_id.hex()) # only works in python 3.5+
    print("DB_RX_Comm: Communication ID: " + str(comm_id))
    dbprotocol = DBProtocol(src, UDP_Port_TX, IP_TX, 0, b'\x03', DB_INTERFACE, mode, comm_id, frame_type, b'\x04')

    while True:
        dbprotocol.receive_process_datafromgroundstation() # blocking


if __name__ == "__main__":
    main()
