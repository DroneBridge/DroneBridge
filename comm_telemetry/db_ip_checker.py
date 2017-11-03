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
        self.sem.acquire()
        ip = str(self.memory.read(key_smartphone_ip_sm).strip(), 'utf-8')
        self.sem.release()
        return ip


def find_smartphone_ip():
    try:
        interfaces = netifaces.interfaces()
        for inter in interfaces:
             if inter == "usb0":
                 time.sleep(3)
                 g = netifaces.gateways()
                 return g["default"][netifaces.AF_INET][0]
        return "192.168.2.2    "
    except KeyError:
        print("IP_CHECKER: KeyError")
        return "192.168.29.129"


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

    while(True):
        time.sleep(2)
        sem.acquire()
        memory.write(find_smartphone_ip())
        sem.release()


if __name__ == "__main__":
    main()
