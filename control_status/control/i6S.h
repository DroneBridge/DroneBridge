//
// Created by cyber on 01.12.17.
//

#ifndef CONTROL_I6S_H
#define CONTROL_I6S_H

#define DEFAULT_i6S_CALIBRATION     "jscal -u 6,0,1,2,3,4,5,16,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303 /dev/input/js0"
#define i6S_descriptor              "Flysky FS-i6S emulator"

int i6S(int Joy_IF, char calibrate_comm[]);

#endif //CONTROL_I6S_H