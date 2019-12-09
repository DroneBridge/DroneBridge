Wifibroadcast real-time video viewer for Raspberry Pi3


The program is a modified version of the Hello_video demo. The goal is stutter-free video.

[DroneBridge Rx] -> [Linux stdout -> stdin Fifo] -> hello_video.bin.xx -> [60FPS hdmi monitor]

The program is in pre-beta status.
It's made exclusively for 60FPS hdmi monitors, so the Linux output must set to 60FPS.

There are 2 versions, choose according to the Tx fps setting:

hello_video.bin.40
It is tested with Rodizio default settings and:
- 48FPS Tx (v1 camera) and 60FPS hdmi monitor
  Latency is about 140msec.
- 59.9FPS Tx (v2 camera) and 60FPS hdmi monitor
(The 59.9 is an ugly workaround, because the program on 60FPS input doesn't handle yet the latency drift, so the slower Tx setting solves the possible crystal inaccuracy drift. Next version will handle that.)


hello_video.bin.30
It is tested with:
30FPS Tx (v1 camera) and 60FPS hdmi monitor
Latency is 6 frames, 200msec. In the first few seconds maybe more, but decreases to 6 frames in 3-5 seconds.


Files can be compiled using the makefile under `/opt/vc/src/hello_pi/` of the raspbian image.

# License

All files in this directory (hello_video.c.xx*) were contributed by mmormota to the EZ-WifiBroadcast project.

```
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
