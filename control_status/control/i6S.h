//
// Created by cyber on 01.12.17.
//

#ifndef CONTROL_I6S_H
#define CONTROL_I6S_H

#define DEFAULT_i6S_CALIBRATION     "jscal -s 6,1,0,125,125,4329472,4194176,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,127,127,4260750,4260750,1,0,120,120,4511382,4036500 /dev/input/js0"
#define i6S_descriptor              "Flysky FS-i6S emulator"

int i6S(int Joy_IF, char calibrate_comm[]);

#endif //CONTROL_I6S_H