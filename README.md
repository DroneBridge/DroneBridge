
# DroneBridge
![DroneBridge](https://raw.githubusercontent.com/seeul8er/DroneBridge/nightly/wiki/DroneBridgeLogo_text.png)

DroneBridge is a system based on the
[WifiBroadcast](https://befinitiv.wordpress.com/wifibroadcast-analog-like-transmission-of-live-video-data/) approach.
A bidirectional digital radio link between two endpoints is established using standard WiFi hardware and a custom protocol.
DroneBridge is optimized for use in UAV applications and is a complete system. It is intended be a real alternative to
other similar systems, such as DJI Lightbridge or OcuSync.

DroneBridge features support for **Raspberry Pi**, **ESP32** on the UAV/ground station side and an **android app**.

Visit **["Not just another drone project"](http://wolfgangchristl.de/not-just-another-drone-project/)** for additional information about the project and its goals

## Releases
Please read the **[getting started guide](https://dronebridge.gitbook.io/docs/dronebridge-for-raspberry-pi/getting-started)**
The v0.7.0 release is recommended.

**Disclaimer: Use at your own risk. Malfunction and sudden signal loss can not be ruled out. Use with caution! Do not fly over people or animals.
The user is responsible for:**
  - **Operating the system within legal limits (e. g. frequency, equivalent isotropically radiated power (EIRP) etc.)**
  - **Any harm or damage caused by using the provided software or parts of it.**

### v0.7.0 Alpha
Use with caution. RC link has a too high latency. USB connection to DroneBridge for Android is unstable.

**[DroneBridge Alpha v0.7.0 alpha Image for Raspberry Pi](https://wolfgangchristl.de/downloads/DroneBridge_0_7_0_alpha.zip)**

**[DroneBridge for Android 2.0.2](https://mega.nz/file/24cQhTxb#P_DlfOWyTOzVoh1mtspO6zeJv7dfzB0DSHN5_x0SxBM)**

## One System. One digital radio link to rule them all.
![DroneBridge concept](https://github.com/DroneBridge/DroneBridge/blob/master/wiki/oneforall.jpg)

* **300 m - 14+ km range*** (500 m - 2 km with standard hardware)
* **1080p video**
* **110ms glass to glass latency** (using android app)
* **Cheap**: starting at 80€ for hardware
* **12 channel RC**
* **MAVLink support** - LTM telemetry deprecated. Use with mwptools, QGroundControl, Mission Planner etc.
* **iNAV** & **MAVLink** based flight controller support
* **Bidirectional**
* Fully integrated **app for Android**
* **OSD**
* **Modular - Write your own powerful plugins**

*Range strongly depends on your setup, environment and legal framework.

## DroneBridge for Android

![DroneBridge for Android app interface](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/dp_app-map-2017-10-29-kleiner.png)

* Easy to use UI & end point of the whole DroneBridge system
* Low latency video decoding
* Change settings, calibrate the RC, view telemetry

**[Learn more about the app](https://dronebridge.gitbook.io/docs/dronebridge-for-android/dronebridge-for-android)**

## Exemplary hardware setup
DroneBridge is available for the Raspberry Pi & ESP32 (no video, telemetry only - WiFi based)
By compiling the libraries on your Linux computer any device can become an AIR or GND unit. This means DroneBridge is not restricted to the Raspberry Pi.
However many single board computers do not offer the same kind of stability and hardware/software support as the Raspberry Pi (camera, H.264 en-/decoding etc.).

### Raspberry Pi/Linux (Long Range Setup)
![DroneBridge long range hardware setup](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/longrange_setup.png)

### DroneBridge for ESP32
For further information have a look at the [DB for ESP32 main page](https://github.com/DroneBridge/ESP32)
![DroneBridge for ESP32 hardware setup](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/db_ESP32_setup.png)

## DroneBridge Modules

DroneBridge is highly modularized to provide flexebility and make development easy. There is a common library for Python 3 and C/C++ that handles everything involving the DroneBridge raw protocol.
It configures the sockets, inits the protocol and provides methods for easy transmission.
Instead of the Android app any other GCS can be used.

[Read more in the Wiki](https://dronebridge.gitbook.io/docs/developer-guide/dronebridge-lib-example-usage)

## System Architecture
![System Architecture](wiki/DB_GCS_NetConf.jpg)

[Read more in the wiki](https://dronebridge.gitbook.io/docs/developer-guide/system-architecture)

## Coming Up:
 - More documentation
 - Add MavLink and MSP waypoint missions editor to Android App
 - DroneBridge Cockpit: A client/OS for x86 systems to monitor and control your UAV
 - See **[milestones](https://dronebridge.gitbook.io/docs/dronebridge-for-raspberry-pi/milestones)**

## You are a developer?
Check out the **[wiki](https://dronebridge.gitbook.io/docs/)**
Check out the **[milestones](https://dronebridge.gitbook.io/docs/dronebridge-for-raspberry-pi/milestones)** and suggest new ones!
Join the **[Gitter room](https://gitter.im/DroneBridge/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link)** and discuss issues, ask questions or give feedback

There is a nightly branch with the most up to date code! It is not guaranteed that the code in that branch is working or even compiling!

**Feel free to ask questions & contribute**


## Licenses

 * All of the code (especially DroneBridge modules & lib_dbcommon - C or Python) is under the Apache 2 license if not specified otherwise
 * WifiBroadcast legacy code (video, OSD, .profile, hello_video) is licensed as specified or under the GPL v2 license
 * All Kernel drivers/patches are under GPL v2 license
