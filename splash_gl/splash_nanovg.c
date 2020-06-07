/*
 *   This file is part of DroneBridge: https://github.com/DroneBridge/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
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

#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <nanovg/nanovg.h>
#define NANOVG_GL2_IMPLEMENTATION	// Use GL2 implementation.
#include "nanovg/nanovg_gl.h"

int main(int argc, char *argv[]) {
    int image_width = 920;
    int image_height = 150;
    int background = 1; // [1 = YES]

    if (argc == 4) {
        image_width = atoi(argv[1]);
        image_height = atoi(argv[2]);
        background = atoi(argv[3]);
    }

    int32_t width, height;
    char filepath[] = {"/home/pi/DroneBridge/splash/db_splash.jpg"};
    struct NVGcontext* vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    return 0;
}
