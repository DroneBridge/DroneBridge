# DroneBridge

A wifibroadcast extension to make Betaflight/Cleanflight/iNAV based flight controllers more usable for aerial photography. Intended to be a real alternative to DJI Lightbridge and other similar systems. Videodownlink and telemetry is provided by wifibroadcast. RC, smartphone app and command modules are provided by this project.

Visit <b>http://wolfgangchristl.de/2017/not-just-another-drone-project/</b> for additional information about the project and its goals


<h3>Control Module</h3>

Programms for TX (groundstation) and RX (drone) to control the drone. The MultiWii Serial Protocol (MSP) is used. In wifi-mode UDP packets with a MSP-Message as content can be sent to RX. Those get passed through to specified USB-port towards FC. WIFI-Mode is not yet implemented


In monitor mode packets get sent without the use of libpcap using raw interfaces. On the receiving side a BPF (Berkely Packet Filter) is used to pass only airbridge-control packets to the receiving socket. The code is optimised for minimal system calls.

<b>Features:</b>
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


<h3>Communication Module</h3>

Currently in alpha phase. Some features untested. Some key parameters are still hardcoded at the beginning of each file. Place TX files in groundstation and RX files on drone.
Check out code here: <b>https://github.com/seeul8er/DroneBridge_Comm</b>
