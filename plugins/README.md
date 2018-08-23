# DroneBridge plugins
Since DroneBridge v0.6 plugins are supported. With plugins the user can easily add own custom scripts to his default DroneBridge image that will execute on startup of the ground station or UAV. Every UAV project is different and every user has its own ideas and plans for his drone. To account for those needs users will want to add custom programmes and functionalities to DroneBridge. With the plugin system of DroneBridge people can add their own software without having to edit the base image. As soon as a new DroneBridge release comes out all you have to do is reinstall your plugins and you are ready to go. No need to merge your custom programmes into the newly released base image.

Examples for plugins are: Trigger actions with your RC hardware switches using DroneBridge RC, custom telemetry parsers, Raspberry Pi PWM output for fan control, ...


## Installation
To install a plugin just download the corresponding ```*.zip``` file and extract its contents to your ```/boot/plugins/``` directory. If you are using Windows you can find the folder in the root folder of the mounted DroneBridge patition. It is the one that appears in the explorer when you insert your SD card into the computer.


## Develop your own DroneBridge plugins
It is very easy to develop and include your own scripts/modules/programms for DroneBridge. Just follow the steps below.

### Structure
The main folder should have the same name as your plugin. Inside that folder put a README.md and `settings.ini` file. The `settings.ini` will be read on startup by the DroneBridge plugin module. The file contains the commands to execute on either ground station or UAV to start the plugin. See further down for detailed information.

```
folder:name_of_your_plugin
│   README.md
│   settings.ini
│   your_script.py
|   your_data.bin
```

### settings.ini
The `settings.ini` contains the following information:

*   Name of the plugin
*   Version of the plugin
*   Author
*   License
*   Start command to run on ground station
*   Start command to run on UAV

**Important: All of the fields below must exist. They need to be set. ```startup_comm=``` can be empty. Do not set any value**

**Example file:**
```INI
[About]
name=ExamplePlugin
version=1
author=seeul8er
license=Apache License 2.0
website=github.com/seeul8er/DroneBridge/plugins/example_plugin

[ground]
startup_comm=python3 your_script.py -ground
[uav]
startup_comm=python3 your_script.py -uav
```

**Variable types**
All values must be Strings except the version. Version must be an Integer.

The startup_comm is executed via python: `subprocess.Popen(startup_comm, shell='True')`

### Accessing telemetry stream
**UAV:** Currently, there is no (fast/simple) way of accessing the telemetry stream on the UAV. You would need to connect to an additional serial port of you FC and get telemetry via that one. Alternatively you can open a raw socket that listens for packets with destination DroneBridge telemetry port (port: 0x02 - or any other port/packet format in case you want different information). You can use the DroneBridge libs to do that (untested). The Linux Kernel and raw sockets allow you to monitor not just the received packets of the wifi adapters, but also all outgoing packets

**Ground station:** Use the DroneBridge C or Python lib to open a new socket that listens for packets with destination DroneBridge telemetry port (port: `0x02`). That way you are sort of creating your own DB-Module. You can have multiple sockets listening for the same DB raw-packets.

### Accessing DroneBridge RC channel values
**UAV:** Use the C-lib to open the shared memory segment for the RC values. Before the RC values get sent to the AirPi and before they are written to the flight controller, the control module copies them to the shared memory. That way other applications can read the current channel values sent via DroneBridge RC.

**Ground station:** same as with UAV

### Other data
One can always open their own raw socket (using DroneBridge lib or custom code) to listen for all kinds of packets sent or received via the wifi adapters. That way all data that is transported can be accessed. However, you will need to do most of the parsing/data processing yourself.
