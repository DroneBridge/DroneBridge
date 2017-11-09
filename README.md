# DroneBridge
![alt text](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/DroneBridgeLogo-1_smaller.png)

A wifibroadcast extension to make Betaflight/Cleanflight/iNAV based flight controllers more usable for aerial photography. Intended to be a real alternative to DJI Lightbridge and other similar systems. Videodownlink and telemetry is provided by wifibroadcast. RC, smartphone app, alternative LTM downlink and command modules are provided by this project.

<h2>One frequency. One digital radio link to rule them all.</h2>
<img src="https://github.com/seeul8er/DroneBridge/blob/master/wiki/oneforall.jpg">

Visit <b>http://wolfgangchristl.de/2017/not-just-another-drone-project/</b> for additional information about the project and its goals

<h2>DroneBridge Beta 0.1 release</h2>
Get it here: https://github.com/seeul8er/DroneBridge/releases/tag/v0.1

To set it up please read the wiki and check out: https://github.com/bortek/EZ-WifiBroadcast/wiki#installation--setup

Corresponding Android App follows during the next days.

<h2>DroneBridge Android App</h2>

Status: Working code. WIP. Soon to be released for beta testing

![alt text](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/dp_app-map-2017-10-29-kleiner.png)

<b>Features:</b>
 - Display low latency video stream of wifibroadcast (USB Tethering or Wifi-AP)
 - Implementation of DroneBridge communication protocol
 - Display RC link quality from DroneBridge control module
 - User Interface to change settings of DroneBridge modules and Wifibroadcast
 - Full support for LTM-Protocol including DroneBridge LTM frame
 - Show home point and drone on map
 - Calculate battery percent from ampere or voltage using a battery model
 - Show Wifibroadcast status: bad blocks/lost packets - bitrate and link quality
 - Display distance between pilot (app) and drone
 - Support for MSPv1 (Betaflight/Cleanflight) and MSPv2 (iNAV)

<b>Comming up...</b>
 - switch between GoPro preview and pi cam
 - creation and upload of waypoint missions
 - support for EZgui an mwp planner mission files
 - ...

<h2>Control Module</h2>

Programms for TX (groundstation) and RX (drone) to control the drone (RC link). The MultiWii Serial Protocol (MSP) is used.
In monitor mode packets get sent without the use of libpcap using raw interfaces. On the receiving side a BPF (Berkely Packet Filter) is used to only forward dronebridge-control packets to the receiving socket. The code is optimised for minimal system calls and lowest latency possible.

<b>Features:</b>
 - completely tested
 - fast
 - secure and save: 
   - RX side is a MSP-passthrough. If no packets arrive nothing is sent to FC just like a "real" RC
   - detection of unplugged RC - simple replugging is possible
   - custom ID for every frame sent. Allowing for multiple pilots to control their drones without need for reconfiguration. It is nearly impossible to accendantly control one others drone
 - auto calibration (needs to be calibrated once)
 - any MSP command for FC can be sent to RX and gets passed on (like ACC/MAG calibration, missions etc.)
 - easy integration of other RCs (currently i6S)
 - supported by all betaflight/cleanflight based FC software
 - support of MSPv2 protocol (iNAV)
 - custom (raw) communication protocol for more security and less cpu usage
 - SDL not required
 - full XBOX controller support (future release)


<h2>Communication Module</h2>

Currently in beta phase.
Nightly code can be found here: <b>https://github.com/seeul8er/DroneBridge_Comm</b>
Allows to change settings of Wifibroadcast and DroneBridge modules on drone and groundstation using the DroneBridge App.
Implements a custom DroneBridge LTM frame to pass extra information about RSSI to the user interface (app)

<h2>Status Module</h2>
Reports Wifibroadcast status to DroneBridge App using a variant ($TZ) of DroneBridge LTM Frame. May replace telemetry module in the future.


<h2>Future Milestones</h2>

 - release a working beta version
 - release beta version of android app
 - integrate GoPro into project
 - implement a custom USB protocol based on android USB accessory to reduce latency
 - implement VR support. Gimbal and yaw of drone can be controlled by head movement
