# DroneBridge plugins

Since DroneBridge v0.4.1 plugins are supported. With plugins the user can easily add own custom scripts to his default DroneBridge image that will execute on startup of the ground station or UAV.

Examples for plugins are: Custom telemetry parsers, Raspberry Pi PWM output for fan control, ...


## Installation

To install a plugin just download the corresponding ```*.zip``` file and extract its contents to your ```/boot/plugins/``` directory. If you are using Windows you can find the folder in the root folder of the mounted DroneBridge patition. It is the one that appears in the explorer when you insert your SD card into the computer.


## Develop your own DroneBridge plugins

It is very easy to develop and include your own scripts for DroneBridge. Just follow the steps below.

### Structure

The main folder should have the same name as your plugin. Inside that folder put a README.md and `settings.ini` file. The `settings.ini` will be read on startup by the DroneBridge startup script. It contains the commands to execute on either ground station or UAV to start the plugin. See further down for detailed information.

```
folder:name_of_your_plugin
│   README.md
│   settings.ini
│   your_script.py
```

### settings.ini

The `settings.ini` contains the following information:

*   Name of the plugin
*   Version of the plugin
*   Author
*   License
*   Startup command on ground station
*   Startup command on UAV

**Important: All of the fields below must exist. They need to be set. ```startup_comm``` can be ```""```**

**Example file:**

```INI
[About]
name="ExamplePlugin"
version=1
author="seeul8er"
license="Apache License 2.0"
website="github.com/seeul8er/DroneBridge/plugins/example_plugin"

[ground]
startup_comm="python3 your_script.py -ground"
[uav]
startup_comm="python3 your_script.py -uav"
```
