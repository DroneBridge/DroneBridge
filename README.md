# DroneBridge
![alt text](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/DroneBridgeLogo-1_smaller.png)

A wifibroadcast extension to make iNAV & and MAVLink based flight controllers more usable for aerial photography. Intended to be a real alternative to DJI Lightbridge and other similar systems. Videodownlink and telemetry is provided by wifibroadcast. RC, smartphone app, alternative telemetry downlink and command modules are provided by this project.

<b>Try iNAV and support the development!</b>

Many people use Pixhawk based flight controllers for their UAVs. While Pixhawk is a excellent platform for aerial photography and other tasks it is also very expensive compared to iNAV based flight controllers. The iNAV Project aims to bring UAV capabilities to Cleanflight/Betaflight and has been very successfully so far. You can get high quality iNAV compatible FCs from 30€ an on, compared to the 60€ for a Pixracer which may has less interfaces or the hundreds of dollars for a Pixhawk 2.1.


<h2>One frequency. One digital radio link to rule them all.</h2>
<img src="https://github.com/seeul8er/DroneBridge/blob/master/wiki/oneforall.jpg">

Visit <b>http://wolfgangchristl.de/2017/not-just-another-drone-project/</b> for additional information about the project and its goals

**What range can one expect?**
Range is the same as with the WifiBroadcast project. Reported ranges are:
* [2.4Ghz] [3dbi omni antennas] [70mw]: ~1km
* [2.4Ghz] [3dbi omni antennas][300mW high-power cards]: ~2km
* [5Ghz][3dbi omni antennas][25mW]: ~250m
* [5Ghz][3dbi omni antennas][300mW high-power cards]: ~1km

Range strongly depends on your setup and environment. The user must ensure that the system is operated within the legal framework of the respective country.

<h2>DroneBridge Beta 0.3 release coming soon!</h2>

**[Download DroneBridge Image for Raspberry Pi](https://github.com/seeul8er/DroneBridge/releases/tag/v0.2)**

**[Download DroneBridge Android App](https://forstudents-my.sharepoint.com/personal/ga25puh_forstudents_onmicrosoft_com/_layouts/15/guestaccess.aspx?docid=06b1ff2fa69744f45921789b52f88d853&authkey=AeCN4yiqgmL06Mq-rO1Lz6Y&expiration=2018-01-31T23%3A00%3A00.000Z&e=7422989d8eee49e28a268767350b10b1)**

To set it up please read the wiki and check out: https://github.com/bortek/EZ-WifiBroadcast/wiki#installation--setup


**Discalmer: Use at your own risk. Malfunction and sudden signal loss can not be ruled out. Use with caution! Do not fly over people or animals. The pilot is responsible for any harm or damage caused by using the provided software or parts of it.**

<h2>DroneBridge Android App</h2>

![DroneBridge Android App interface](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/dp_app-map-2017-10-29-kleiner.png)

<b>Features:</b>
 - Display low latency video stream of wifibroadcast (USB Tethering or Wifi-AP)
 - Implementation of DroneBridge communication protocol
 - Display RC link quality from DroneBridge control module
 - User Interface to change settings of DroneBridge modules and Wifibroadcast
 - Full support for LTM-Protocol
 - Full support for MAVLink Telemetry
 - Show home point and drone on map
 - Calculate battery percent from ampere or voltage using a battery model
 - Show Wifibroadcast status: bad blocks/lost packets - bitrate and link quality
 - Display distance between pilot (app) and drone
 - Support for MSPv1 (Betaflight/Cleanflight) and MSPv2 (iNAV)

<h2>Exemplary hardware setup</h2>

![possible hardware setup](https://raw.githubusercontent.com/seeul8er/DroneBridge/nightly/wiki/Hardware_setup.png)

Other configurations where a laptop functions as the groundstation or a HDMI screen/goggles are connected to the ground Pi should be possible but are untested. Other SBC might work.

<h2>DroneBridge Modules</h2>

DroneBridge is highly modularized to provide flexebility and make development easy. There is a common library for Python3 and C that handles everything involving the DroneBridge raw protocol. It configures the sockets, inits the protocol and provides methods for easy transmission.

<h3>Control Module</h2>

You can use the DroneBridge Android app to see what the control module is reading form your RC (channel data).

<b>Features:</b>
 - Tested
 - Fast
 - Secure and safe by design: 
   - RX side can act as a passthrough or interpreter for DB RC packet to MSP. If no packets arrive nothing is sent to FC just like a "real" RC
   - Detection of unplugged RC - simple replugging is possible
   - Custom ID for every frame sent. Allowing for multiple pilots to control their drones without need for reconfiguration. It is nearly impossible to accendantly control one others drone
   - All data is checked by a appropriate CRC8
 - 12 channels using DroneBridge RC packet
 - Auto calibration (needs to be calibrated once)
 - Any MSP command for FC can be sent to RX and gets passed on (like ACC/MAG calibration, missions etc.)
 - Easy integration of other RCs (currently i6S)
 - Supported by all betaflight/cleanflight based FC software including iNAV (MSP v2)!
 - Custom (raw) communication protocol for more security and less cpu usage

Discalmer: Malfunction and sudden signal loss can not be ruled out. Use with caution! Do not fly over people or animals. The pilot is responsible for any harm or damage caused by using this software or parts of it.

<h3>Communication Module</h3>

Allows to change and request settings of WifiBroadcast and DroneBridge modules on drone and groundstation using the DroneBridge app.

<h3>Status Module</h3>

 - Reports Wifibroadcast status to DroneBridge app
 - Reports the RC channels sent via DroneBridge control module to DroneBridge app

<h2>Future Milestones</h2>

### Blackbox concept
![Blackbox](https://github.com/seeul8er/DroneBridge/blob/master/wiki/Blackbox.png)

## coming up:
 - more documentation
 - add MavLink and MSP waypoint missions
 - make DroneBridge more indipendent of WifiBroadcast (video) > support of esp32 or esp8266 modules to allow integration in existing builds using analog video > no need for raspberry pi: This will provide all features but video. Range will be reduced to 150-300m (no antenna mod) as we use esp-modules to do transmission. If you want video you will need your standard analog FPV cameras and googles etc.. Support for analog video grabbers might be added to android app.
 - (integrate GoPro into project) - wifi seems to interfere with GPS- might be a bad idea...
 - implement VR support. Gimbal and yaw of drone can be controlled by head movement
 - See **[milestones](https://github.com/seeul8er/DroneBridge/wiki/Milestones)**

<h2>You are a developer?</h2>

Check out the **[wiki](https://github.com/seeul8er/DroneBridge/wiki)**

Check out the **[milestones](https://github.com/seeul8er/DroneBridge/wiki/Milestones)** and suggest new ones!

Join the **[Gitter room](https://gitter.im/DroneBridge/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link)** and discuss issues, ask questions or give feedback

There is a nightly branch with the most up to date code! It is not guaranteed that the code in that branch is working or even compiling!

**Feel free to ask questions and criticize each and everything!**
