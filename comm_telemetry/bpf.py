# Credit goes to https://github.com/dw/scratch/blob/master/graceful_unlisten/server.py
# Thanks for sharing! :)

import ctypes
from socket import *

SO_ATTACH_FILTER = 26

class BpfProgram(ctypes.Structure):
    _fields_ = [
        ('bf_len', ctypes.c_int),
        ('bf_insns', ctypes.c_void_p)
    ]

class BpfInstruction(ctypes.Structure):
    _fields_ = [
        ('code', ctypes.c_uint16),
        ('jt', ctypes.c_uint8),
        ('jf', ctypes.c_uint8),
        ('k', ctypes.c_uint32)
    ]

def attach_filter(sock, direction, dst_mac, port):
    """dst_mac is the mac of local wifi interface (destination of packet) which is what we declared in src_mac"""
    # first byte of dst_mac must be 0x01, we overwrite it here to be sure
    version_port = b'\x01'+port
    dest_mac2 = int.from_bytes(dst_mac[2:6], byteorder='big', signed=False)
    dest_mac1 = int.from_bytes(bytearray(b'\x01' + direction), byteorder='big', signed=False)
    BPFFILTER = [
        [0x30, 0, 0, 0x00000003],
        [0x64, 0, 0, 0x00000008],
        [0x07, 0, 0, 0000000000],
        [0x30, 0, 0, 0x00000002],
        [0x4c, 0, 0, 0000000000],
        [0x02, 0, 0, 0000000000],
        [0x07, 0, 0, 0000000000],
        [0x50, 0, 0, 0000000000],
        [0x45, 1, 0, 0x00000008], # allow data frames
        [0x45, 0, 9, 0x00000080], # allow beacon frames
        [0x40, 0, 0, 0x00000006],
        [0x15, 0, 7, dest_mac2], # <comm_id>
        [0x48, 0, 0, 0x00000004],
        [0x15, 0, 5, dest_mac1], # <odd><direction>
        [0x48, 0, 0, 0x00000010],
        [0x15, 0, 3, int.from_bytes(version_port, byteorder='big', signed=False)], # e.g. version = 0x01 port = 0x02
        [0x50, 0, 0, 0x00000012],
        [0x15, 0, 1, int.from_bytes(direction, byteorder='big', signed=False)],
        [0x06, 0, 0, 0x00040000],
        [0x06, 0, 0, 0000000000],
    ]

    insns = (BpfInstruction * len(BPFFILTER))()
    for i, (code, jt, jf, k) in enumerate(BPFFILTER):
        insns[i].code = code
        insns[i].jt = jt
        insns[i].jf = jf
        insns[i].k = k

    prog = BpfProgram()
    prog.bf_len = len(BPFFILTER)  # Opcode count
    prog.bf_insns = ctypes.addressof(insns)
    e = sock.setsockopt(SOL_SOCKET, SO_ATTACH_FILTER, prog)
    print("Tried to attach BPF to socket. returned: "+str(e))
    return sock
