#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "main.h"
#include "parameter.h"

#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
#define JS_EVENT_AXIS           0x02    /* joystick moved */
#define JS_EVENT_INIT           0x80    /* initial state of device */

#define MAX 32767

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

int initialize_i6S(int new_Joy_IF, char calibrate_comm[]) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    printf("Waiting for i6S to be detected on: %s\n", interface_joystick);
    do {
        usleep(250);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keepRunning);
    printf("Opened joystick interface!\n");
    printf("Calibrating...\n");
    int returnval = system(calibrate_comm);
    if (returnval == 0) {
        printf("Calibrated i6S\n");
    }else{
        printf("Could not calibrate i6S\n");
    }
    return fd;
}

int i6S(int Joy_IF, char calibrate_comm[]) {
    signal(SIGINT, intHandler);
    unsigned short JoystickData[NUM_CHANNELS];
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 16666666L; //60Hz
    //tim.tv_nsec = 10000000L; //100Hz (should be better for monitor mode and packet loss)


    struct js_event {
        unsigned int time;      /* event timestamp in milliseconds */
        short value;            /* value */
        unsigned char type;     /* event type */
        unsigned char number;   /* axis/button number */
    };

    struct i6SRC {
        int16_t roll;
        int16_t pitch;
        int16_t throttle;
        int16_t yaw;
        int16_t cam_up;
        int16_t cam_down;
        int16_t button0;
        int16_t button1;
        int16_t button2;
        int16_t button3;
        int16_t button4;
        int16_t button5;
        int16_t pos_switch1;
        int16_t pos_switch2;
    };

    int fd = initialize_i6S(Joy_IF, calibrate_comm);

    struct js_event e;
    struct i6SRC rc;
    rc.roll = 0;
    rc.pitch = 0;
    rc.throttle = 0;
    rc.yaw = 0;
    rc.cam_up = 0;
    rc.cam_down = 0;
    rc.button0 = 1;
    rc.button1 = 0;
    rc.button2 = 1;
    rc.button3 = 0;
    rc.button4 = 1;
    rc.button5 = 0;
    rc.pos_switch1 = 1000;
    rc.pos_switch2 = 1000;

//    int count = 0;
//    struct timeval  tv;
//    gettimeofday(&tv, NULL);
//    double begin = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
    printf("Starting to send commands!\n");
    while (keepRunning) //send loop
    {
        nanosleep(&tim, &tim2);
        while (read(fd, &e, sizeof(e)) > 0)   // go through all events occurred
        {
            e.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
            if (e.type == JS_EVENT_AXIS) {
                switch (e.number) {
                    case 0:
                        rc.roll = e.value;
                        break;
                    case 1:
                        rc.pitch = e.value;
                        break;
                    case 2:
                        rc.throttle = e.value;
                        break;
                    case 3:
                        rc.yaw = e.value;
                        break;
                    case 4:
                        rc.cam_up = e.value;
                        break;
                    case 5:
                        rc.cam_down = e.value;
                        break;
                    default:
                        break;
                }
            } else if (e.type == JS_EVENT_BUTTON) {
                switch (e.number) {
                    case 0:
                        rc.button0 = e.value;
                        break;
                    case 1:
                        rc.button1 = e.value;
                        break;
                    case 2:
                        rc.button2 = e.value;
                        break;
                    case 3:
                        rc.button3 = e.value;
                        break;
                    case 4:
                        rc.button4 = e.value;
                        break;
                    case 5:
                        rc.button5 = e.value;
                        break;
                    default:
                        break;
                }
            }
        }
        int myerror = errno;
        if (myerror != EAGAIN) {
            if (myerror == ENODEV) {
                printf("Joystick was unplugged! Retrying...\n");
                fd = initialize_i6S(Joy_IF, calibrate_comm);
            } else {
                printf("Error: %s\n", strerror(myerror));
            }
        }
        // SWR - Arm switch
        if (rc.button0 == 1) { rc.button0 = 1000; } else if (rc.button0 == 0) { rc.button0 = 2000; }
        // SWD - failsafe
        if (rc.button5 == 0) { rc.button5 = 1000; } else if (rc.button5 == 1) { rc.button5 = 2000; }

        // SWB - 3pos switch 1
        if (rc.button1 == 0 && rc.button2 == 1) {
            rc.pos_switch1 = 1000;
        } else if (rc.button1 == 0 && rc.button2 == 0) {
            rc.pos_switch1 = 1500;
        } else {
            rc.pos_switch1 = 2000;
        }

        // SWC - 3pos switch 2
        if (rc.button3 == 0 && rc.button4 == 1) {
            rc.pos_switch2 = 1000;
        } else if (rc.button3 == 0 && rc.button4 == 0) {
            rc.pos_switch2 = 1500;
        } else {
            rc.pos_switch2 = 2000;
        }
        //adjust endpositions and buttons
        if (rc.roll == 32766) rc.roll++;
        if (rc.pitch == 32766) rc.pitch++;
        if (rc.throttle == 32766) rc.throttle++;
        if (rc.yaw == 32766) rc.yaw++;
        // TODO: Check if there is input from the socket/FIFO (smartphone could send some extra commands)
//        printf( "%c[;H", 27 );
//        printf("Roll:     %i          \n",normalize_i6S(rc.roll,500));
//        printf("Pitch:    %i          \n",normalize_i6S(rc.pitch,500));
//        printf("Throttle: %i          \n",normalize_i6S(rc.throttle,500));
//        printf("Yaw:      %i          \n",normalize_i6S(rc.yaw,500));
//        printf("Cam up:   %i          \n",normalize_i6S(rc.cam_up,500));
//        printf("Cam down: %i          \n",normalize_i6S(rc.cam_down,500));
//        printf("Button 0:     %i          \n",rc.button0);
//        printf("pos_switch 1: %i          \n",rc.pos_switch1);
//        printf("pos_switch 2: %i          \n",rc.pos_switch2);
//        printf("Button 5:     %i          \n",rc.button5);

        JoystickData[0] = htons(normalize_i6S(rc.roll, 500));
        JoystickData[1] = htons(normalize_i6S(rc.pitch, 500));
        JoystickData[2] = htons(normalize_i6S(rc.yaw, 500));
        JoystickData[3] = htons(normalize_i6S(rc.throttle, 500));
        JoystickData[4] = htons(normalize_i6S(rc.cam_up, 500));
        JoystickData[5] = htons(normalize_i6S(rc.cam_down, 500));
        JoystickData[6] = htons(rc.button0);
        JoystickData[7] = htons(rc.pos_switch1);
        JoystickData[8] = htons(rc.pos_switch2);
        JoystickData[9] = htons(rc.button5);
        JoystickData[10] = htons(1000); // unused by i6s - used by app
        JoystickData[11] = htons(1000); // unused by i6s - used by app
        JoystickData[12] = htons(1000); // unused by i6s - used by app
        JoystickData[13] = htons(1000); // unused by i6s - used by app
        sendPacket(JoystickData);
//            count++;
//            if(count == 120){
//                gettimeofday(&tv, NULL);
//
//                double end = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
//                printf("Duration to send %i times: %f ms",count,end-begin);
//                keepRunning = false;
//            }
    }
    close(fd);
    closeSocket();
    return 0;
}

int16_t normalize_i6S(int16_t value, int16_t adjustingValue) {
    return ((adjustingValue * value) / MAX) + 1500;
}
