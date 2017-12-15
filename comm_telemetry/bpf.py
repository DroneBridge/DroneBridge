# Credit goes to https://github.com/dw/scratch/blob/master/graceful_unlisten/server.py
# Thanks for providing an example! :)

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

def attach_filter(sock, byte_comm_id, byte_direction, byte_port):
    """Build a BPF filter for DroneBridge raw protocol v2"""
    # first byte of dst_mac must be 0x01, we overwrite it here to be sure
    u32_port = int.from_bytes(b'\x00\x00\x00'+byte_port, byteorder='big', signed=False)
    u32_direction_comm = int.from_bytes(b'\x00\x00' + byte_direction + byte_comm_id, byteorder='big', signed=False)
    BPFFILTER =[
                    [0x30,  0,  0, 0x00000003],
                    [0x64,  0,  0, 0x00000008],
                    [0x07,  0,  0, 0000000000],
                    [0x30,  0,  0, 0x00000002],
                    [0x4c,  0,  0, 0000000000],
                    [0x02,  0,  0, 0000000000],
                    [0x07,  0,  0, 0000000000],
                    [0x48,  0,  0, 0000000000],
                    [0x45,  1,  0, 0x0000b400],  # allow rts frames
                    [0x45,  0,  5, 0x00008000],  # allow data frames
                    [0x48,  0,  0, 0x00000004],
                    [0x15,  0,  3, u32_direction_comm],  # <direction><comm id>
                    [0x50,  0,  0, 0x00000006],
                    [0x15,  0,  1, u32_port],  # <port>
                    [0x06,  0,  0, 0x00002000],
                    [0x06,  0,  0, 0000000000],
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
    sock.setsockopt(SOL_SOCKET, SO_ATTACH_FILTER, prog)
    return sock
