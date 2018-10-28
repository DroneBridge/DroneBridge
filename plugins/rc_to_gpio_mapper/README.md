# Example: Maps DroneBridge RC channels to Raspberry Pi GPIO pins

Sets GPIO pins `21, 22, 27` (WiringPi definition) to high if DroneBridge RC channel values are `>=1500`.

**Can be used to set the GPIO pins of the Raspberry Pi via the RC hardware buttons.**


Maps:

* CH10 -> pin 21
* CH11 -> pin 22
* CH12 -> pin 27

A plugin that reads the RC values sent to the FC via DroneBridge RC message and the control module.

This plugin works on UAS and ground station.

## Installation

Copy this folder into the ```/boot/plugins``` directory of your DroneBridge image.
```plugins``` directory can be found in the same place as the config files!
