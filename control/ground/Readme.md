Build using `cmake .` followed by `make`

Options:
* -n network interface for long range 
* -j number of joystick interface of RC
* -m mode: <w|m> for wifi or monitor
* -g a command to calibrate the joystick. Gets executed on initialisation: execute `jscal -c /dev/input/js0` to calibrate/generate calibration data
* -a frame type [1|2] <1> for Ralink und <2> for Atheros chipsets
* -c the communication ID (same on drone and groundstation)
* -b bitrate: 1 = 2.5Mbit 2 = 4.5Mbit 3 = 6Mbit 4 = 12Mbit (default) 5 = 18Mbit (bitrate option only supported with Ralink chipsets)

Example:

`control_tx -n wlan0 -j 0 -m m -a 1 -c aabbccdd -b 3 -g "jscal -s 6,1,0,121,121,5064665,4880496,1,0,122,122,4970875,4970875,1,0,122,122,5212180,4970875,1,0,122,122,4970875,4970875,1,1,126,126,4793344,5162063,1,0,127,127,4750925,5212180 /dev/input/js0"` 
