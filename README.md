# ProjectPegasus_control
Programms for TX (groundstation) and RX (drone) to control the drone. The MultiWii Serial Protocol (MSP) is used. Creates a Socket to be controled by an external App. In wifi-mode raw ethernet packets with a MSP-Message as content can be sent to RX. Those get passed through to specified USB-port towards FC. Ether-Type must be "0x88ab". THIS IS LEGACY MODE


In monitor mode packets get sent without the use of libpcap using raw interfaces. On the receiving side a BPF (Berkely Packet Filter) is used to pass only airbridge-control packets to the receiving socket. The code is optimised for minimal system calls.

Features:
 - fast
 - secure and save: 
   - RX side is a MSP-passthrough. If no packets arrive nothing is sent to FC just like a "real" RC
   - detection of unplugged RC - simple replugging is possible
   - custom ID (MAC of drone wifi interface) for every frame sent allowing for multiple pilots to control their drones without need   for reconfiguration. It is nearly impossible to accendantly control one others drone 
 - any MSP command for FC can be sent to RX and gets passed on (like ACC/MAG calibration etc.)
 - easy integration of other RCs (currently i6S)
 - supported by all betaflight/cleanflight FC software
 - custom (raw) communication protocol
 - SDL2 not required in future release
 - full XBOX controller support (future release)
