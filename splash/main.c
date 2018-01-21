// Show DroneBridge splash screen by Wolfgang Christl
// This file is part of DroneBridge and licensed under Apache 2

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"

int main(int argc, char *argv[]) {

    int image_width = 920;
    image_width = atoi(argv[1]);
    int image_height = 150;
    image_height = atoi(argv[2]);
    int background = 1; // [1 = YES]
    background = atoi(argv[3]);

    int width, height;
    char filepath[] = {"/etc/dronebridge/db_splash.jpg"};

    init(&width, &height);            // Graphics initialization


    Start(width, height);
    float a = 0;
    if (background == 1) { BackgroundRGB(0, 0, 0, 1); };
    Fill(255, 255, 255, 1);
    Image((width-image_width) / 2, (height-image_height) / 2, image_width, image_height, filepath);
    End();
    usleep(7000000);

    finish();                 // Graphics cleanup
    exit(0);
}
