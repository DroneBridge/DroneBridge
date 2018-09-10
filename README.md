# DroneBridge
![DroneBridge](https://raw.githubusercontent.com/seeul8er/DroneBridge/nightly/wiki/DroneBridgeLogo_text.png)

DroneBridge is a system based on the [WifiBroadcast](https://befinitiv.wordpress.com/wifibroadcast-analog-like-transmission-of-live-video-data/) approach. A bidirectional digital radio link between two endpoints is established using standard WiFi hardware and a custom protocol. DroneBridge is optimized for use in UAV applications and is a complete system. It is intended be a real alternative to other similar systems, such as DJI Lightbridge or OcuSync.

DroneBridge features support for **Raspberry Pi**, **ESP32** on the UAV/ground station side and an **android app**.

Visit **["Not just another drone project"](http://wolfgangchristl.de/not-just-another-drone-project/)** for additional information about the project and its goals

## DroneBridge Beta 0.5 released!

**[DroneBridge Beta v0.5 Image for Raspberry Pi](https://github.com/seeul8er/DroneBridge/releases/tag/v0.5)**

**[DroneBridge for Android 1.2.4](https://forstudents-my.sharepoint.com/:u:/g/personal/ga25puh_forstudents_onmicrosoft_com/Ec38kmt91ilNvYx6xnFwwQgBb9UDGDCGt6L34zZZ9YeMlw)**

**[Get started](https://github.com/seeul8er/DroneBridge/wiki/Setup-Guide)**

To set it up please read the wiki and check out: [WifiBroadcast installation guide](https://github.com/bortek/EZ-WifiBroadcast/wiki#installation--setup)


**Discalmer: Use at your own risk. Malfunction and sudden signal loss can not be ruled out. Use with caution! Do not fly over people or animals. The pilot is responsible for any harm or damage caused by using the provided software or parts of it.**

## One System. One digital radio link to rule them all.
![DroneBridge concept](https://github.com/seeul8er/DroneBridge/blob/master/wiki/oneforall.jpg)

* **300 m - 14+ km range*** (500 m - 2 km with standard hardware)
* **1080p video**
* **110ms glass to glass latency** (using android app)
* **cheap**: starting at 80€ for hardware
* **12 channel RC**
* **LTM & MAVLink telemetry** - Use with mwptools, QGroundControl, Mission Planner etc.
* **iNAV** & **MAVLink** based flight controller support
* **bidirectional**
* full featured **Android app**
* **OSD**
* **multi camera support**
* **extendability**

*Range strongly depends on your setup and environment. The user must ensure that the system is operated within the legal framework of the respective country.

<h2>DroneBridge for Android</h2>

![DroneBridge for Android app interface](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/dp_app-map-2017-10-29-kleiner.png)

* Easy to use UI & end point of the whole DroneBridge system
* Low latency video decoding
* Change settings, calibrate the RC, view telemetry from within the app

**[Learn more about the app](https://github.com/seeul8er/DroneBridge/wiki/Android-App)**

## Exemplary hardware setup
### long range setup
![DroneBridge long range hardware setup](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/longrange_setup.png)

### DroneBridge for ESP32
![DroneBridge for ESP32 hardware setup](https://raw.githubusercontent.com/seeul8er/DroneBridge/master/wiki/db_ESP32_setup.png)

## DroneBridge Modules

DroneBridge is highly modularized to provide flexebility and make development easy. There is a common library for Python3 and C that handles everything involving the DroneBridge raw protocol. It configures the sockets, inits the protocol and provides methods for easy transmission.

[Read more in the Wiki](https://github.com/seeul8er/DroneBridge/wiki)

## Blackbox concept
![Blackbox](https://github.com/seeul8er/DroneBridge/blob/master/wiki/Blackbox.png)

## coming up:
 - more documentation
 - add MavLink and MSP waypoint missions
 - make DroneBridge more indipendent of WifiBroadcast
 - DroneBridge Cockpit: A client/OS for x86 systems to monitor and control your UAV
 - See **[milestones](https://github.com/seeul8er/DroneBridge/wiki/Milestones)**

<h2>You are a developer?</h2>

Check out the **[wiki](https://github.com/seeul8er/DroneBridge/wiki)**

Check out the **[milestones](https://github.com/seeul8er/DroneBridge/wiki/Milestones)** and suggest new ones!

Check out the **[Github main project](https://github.com/DroneBridge)** for other flavors

Check out the **[modified Kernel](https://github.com/DroneBridge/RPiKernel)**

Write your own modules/code that runs at startup using **[DroneBridge plugins](https://github.com/seeul8er/DroneBridge/wiki/DroneBridge-plugins)**

Join the **[Gitter room](https://gitter.im/DroneBridge/Lobby?utm_source=share-link&utm_medium=link&utm_campaign=share-link)** and discuss issues, ask questions or give feedback

There is a nightly branch with the most up to date code! It is not guaranteed that the code in that branch is working or even compiling!

**Feel free to ask questions and criticize each and everything!**
