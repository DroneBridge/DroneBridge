/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2017 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

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
