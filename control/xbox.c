#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <time.h>
#include <unistd.h>
#include "control_main_ground.h"

#define MAX             32768
#define CENTERDEAD      5000
#define CENTERDEADN     -5000
#define TRIMSTEP        328             //5er steps: 1 step in 1000-2000 scales to 65,536 in -32768 to 32768

#define DELTAX          (MAX-CENTERDEAD)
#define TPLUS           (((500-2000)*MAX+2000*CENTERDEAD)/(CENTERDEAD-MAX))
#define TMINUS          3000-TPLUS

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
    printf("DB_CONTROL_TX: Waiting for i6S to be detected on: %s\n", interface_joystick);
    do {
        usleep(100000);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0 && keepRunning);
    printf("DB_CONTROL_TX: Opened joystick interface!\n");
    printf("DB_CONTROL_TX: Calibrating...\n");
    int returnval = system(calibrate_comm);
    if (returnval == 0) {
        printf("DB_CONTROL_TX: Calibrated i6S\n");
    }else{
        printf("DB_CONTROL_TX: Could not calibrate i6S\n");
    }
    return fd;
}

int16_t normalize_xbox(int value, int adjustingValue, int axis)
{
    int computed = 0;
    if (value<CENTERDEAD && value>CENTERDEADN)
    {
        computed = 1500;
    }
    else
    {
        if(axis == 1)       //Throttle value should not be adjusted from 1500 to 1000 in order to reach 1000 for landing
        {
            if (value>0)
            {
                computed = ((adjustingValue*value)/DELTAX)+TPLUS;
            }
            else
            {
                computed = ((500*value)/DELTAX)+TMINUS;
            }
        }
        else
        {
            if (value>0)
            {
                computed = ((adjustingValue*value)/DELTAX)+TPLUS;
            }
            else
            {
                computed = ((adjustingValue*value)/DELTAX)+TMINUS;
            }
        }
    }
    if (computed==1999)
    {
        computed = 2000;
    }
    else if(computed==1001)
    {
        computed = 1000;
    }
    printf("%i ",computed);
    return computed;
}

int xbox_one(SDL_Joystick *joystick){
    signal(SIGINT, intHandler);
    unsigned short JoystickData[NUM_CHANNELS];

    Uint16 armedValue = 1000;
    bool keepRunning = true, isarmed = false;
    int adjustingArray[] = {250,350,500};
    int AdjustingValue = 500;
    int sensLevel = (sizeof(adjustingArray) / sizeof(int))-1;  //immer mit der kleinsten Sensitivität loslegen

    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 16666666L; //60Hz
    //tim.tv_nsec = 10000000L; //100Hz

    int a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
    bool b0 = false, b1 = false, b2 = false, b3 = false, b4 = false, b5 = false, b6 = false;
    struct js_event {
        unsigned int time;      /* event timestamp in milliseconds */
        short value;            /* value */
        unsigned char type;     /* event type */
        unsigned char number;   /* axis/button number */
    };
    struct xbox_one {
        int16_t roll;
        int16_t pitch;
        int16_t throttle;
        int16_t yaw;
        int16_t lt;
        int16_t rt;
        int16_t a;
        int16_t b;
        int16_t x;
        int16_t y;
        int16_t lb;
        int16_t rb;
        int16_t start;
        int16_t back;
    };
    int fd = initialize_i6S(Joy_IF, calibrate_comm);
    struct js_event e;
    struct xbox_one rc;
    rc.roll = 0;
    rc.pitch = 0;
    rc.throttle = 0;
    rc.yaw = 0;
    rc.lt = 0;
    rc.rt = 0;
    rc.a = 0;
    rc.b = 0;
    rc.x = 0;
    rc.y = 0;
    rc.lb = 0;
    rc.rb = 0;
    rc.start = 0;
    rc.back = 0;

    printf("DB_CONTROL_TX: Starting to send commands!\n");
    while(keepRunning)
    {
        // TODO: adjust for controller
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
                printf("DB_CONTROL_TX: Joystick was unplugged! Retrying...\n");
                fd = initialize_i6S(Joy_IF, calibrate_comm);
            } else {
                printf("DB_CONTROL_TX: Error: %s\n", strerror(myerror));
            }
        }
        printf( "%c[;H", 27 );

        printf("Sensetifity-Level: %i                               \n",sensLevel+1);
        printf("Armed: %i                                           \n",isarmed);
        if(rc.a==1)
        {
            if(a1<-32600)
            {
                //arm
                a1=-32768;
                a0=32768;
            }
        }
        if(rc.b==1)
        {
            if(a1<-32600)
            {
                //disarm
                a1=-32768;
                a0=-32768;
            }
        }

        if(rc.lb == 1 && rc.rb == 1)
        {
            //keepRunning = false;
        }

        //Adjusting Joystick endpositions
        if (rc.roll == 32766) rc.roll++;
        if (rc.pitch == 32766) rc.pitch++;
        if (rc.throttle == 32766) rc.throttle++;
        if (rc.yaw == 32766) rc.yaw++;

        AdjustingValue = adjustingArray[sensLevel];

        JoystickData[0] = normalize_xbox(rc.roll, AdjustingValue, 3);
        JoystickData[1] = normalize_xbox(rc.pitch, AdjustingValue, 4);
        JoystickData[2] = normalize_xbox(rc.yaw, AdjustingValue, 0);
        JoystickData[3] = normalize_xbox(rc.throttle, AdjustingValue, 1);
        JoystickData[10] = 1000; // unused by i6s - used by app
        JoystickData[11] = 1000; // unused by i6s - used by app
        JoystickData[12] = 1000; // unused by i6s - used by app
        JoystickData[13] = 1000; // unused by i6s - used by app
        send_rc_packet(JoystickData);
    }
    close(fd);
    closeSocket();
    return 0;
}
