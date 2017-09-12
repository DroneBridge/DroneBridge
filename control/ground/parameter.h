#ifndef PARAMETER_H_INCLUDED
#define PARAMETER_H_INCLUDED

//#define DEST_MAC_CHAR   "18:a6:f7:16:a5:11" // TP-Link
// #define DEST_MAC_CHAR   "00:0e:e8:dc:aa:2c" // Zioncom

#define NUM_CHANNELS    14      //number of channels sent over MSP_RC (AERT12345678910)
#define DEFAULT_BITRATE_OPTION 4
#define JOY_INTERFACE   0
//#define DEFAULT_IF      "wlx18a6f716a511"   // TP-Link
#define DEFAULT_IF      "wlx000ee8dcaa2c"   // Zioncom
#define DEFAULT_COMMID "aabbccdd"

#define DEFAULT_i6S_CALIBRATION     "jscal -s 6,1,0,125,125,4329472,4194176,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,120,120,4511382,4036500 /dev/input/js0"
#define i6S_descriptor               "Flysky FS-i6S emulator"

#endif // PARAMETER_H_INCLUDED
