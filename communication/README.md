# DroneBridge Communication Module
* **Request settings** and configuration of drone, ground station and camera including DroneBridge-, GoPro-, WifiBroadcast-Settings
* **Change settings** of DroneBridge, WifiBroadcast and GoPro
* **Request & send firmware/hardware version**
* Easily to be extended to send any command to ground station or drone. Like shell commands

This protocol is based on JSON which can be transported using the DroneBridge Raw protocol (Groundstation <--> Drone), DroneBridge USB protocol (App <--> Groundstation) or standard network protocols like UDP (wifi mode or USB tethering)

## Using DroneBridge Python lib
In on this page, you will find some information on how to make use of DroneBridge for your own projects. Many times you will want to send your own data over the long-range link. You can do that by using the DroneBridge lib (C or Python).

You will need a working DroneBridge setup running on your Raspberry Pis (ground station & UAV)

The Python lib is very messy right now. A lot of code cleanup needs to be done but it works and is very easy to use!

## Sample code
**Before execution make sure you set the wifi interface correctly! (var: interface_drone_comm)**

[Execute as root on your ground station](https://github.com/seeul8er/DroneBridge/blob/master/comm_telemetry/custom_link_gnd.py)

[Execute as root on your UAV](https://github.com/seeul8er/DroneBridge/blob/master/comm_telemetry/custom_link_uav.py)