#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <time.h>
#include <unistd.h>
#include "main.h"

#define MAX             32768
#define CENTERDEAD      5000
#define CENTERDEADN     -5000
#define TRIMSTEP        328             //5er steps: 1 step in 1000-2000 scales to 65,536 in -32768 to 32768

#define DELTAX          (MAX-CENTERDEAD)
#define TPLUS           (((500-2000)*MAX+2000*CENTERDEAD)/(CENTERDEAD-MAX))
#define TMINUS          3000-TPLUS

int xbox_only(SDL_Joystick *joystick){
    unsigned short JoystickData[8];
    Uint16 armedValue = 1000;
    bool keepRunning = true, isarmed = false;
    int adjustingArray[] = {250,350,500};
    int AdjustingValue = 500;
    int sensLevel = (sizeof(adjustingArray) / sizeof(int))-1;  //immer mit der kleinsten Sensitivität loslegen
    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 18181818L; //55Hz

    int a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
    bool b0 = false, b1 = false, b2 = false, b3 = false, b4 = false, b5 = false, b6 = false;

    SDL_Event event;

//    struct timeval  tv;
//    gettimeofday(&tv, NULL);
//    double begin = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
    while(keepRunning)
    {
        SDL_JoystickUpdate();
        a0 = SDL_JoystickGetAxis(joystick, 0);
        a1 = -1*SDL_JoystickGetAxis(joystick, 1);
        a2 = SDL_JoystickGetAxis(joystick, 2);
        a3 = SDL_JoystickGetAxis(joystick, 3);
        a4 = -1*SDL_JoystickGetAxis(joystick, 4);
        a5 = SDL_JoystickGetAxis(joystick, 5);
        b0 = SDL_JoystickGetButton(joystick, 0);
        b1 = SDL_JoystickGetButton(joystick, 1);
//        b2 = SDL_JoystickGetButton(joystick, 2);
//        b3 = SDL_JoystickGetButton(joystick, 3);
        b4 = SDL_JoystickGetButton(joystick, 4);
        b5 = SDL_JoystickGetButton(joystick, 5);
//        b6 = SDL_JoystickGetButton(joystick, 6);
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            case SDL_JOYHATMOTION:
                if (event.jhat.value == SDL_HAT_DOWN)
                {
                    if(sensLevel>0)
                    {
                        sensLevel--;
                    }
                }
                else if(event.jhat.value == SDL_HAT_UP)
                {
                    if(sensLevel<2)
                    {
                        sensLevel++;
                    }
                }
                break;
            case SDL_JOYBUTTONUP:
                if(event.jbutton.button == 7)
                {
                    if(isarmed==true)
                    {
                        isarmed=false;
                        armedValue=1000;
                    }
                    else
                    {
                        isarmed=true;
                        armedValue=2000;
                    }
                }
                break;
            }
        }
        nanosleep(&tim , &tim2);

        printf( "%c[;H", 27 );
//        printf("\n");
//        printf("Axis 0: %i          \n",a0);    //L-LR
//        printf("Axis 1: %i          \n",a1);    //L-UD --> Throttle
//        printf("Axis 2: %i          \n",a2);    //Bremsen
//        printf("Axis 3: %i          \n",a3);    //R-LR
//        printf("Axis 4: %i          \n",a4);    //R-UD
//        printf("Axis 5: %i          \n",a5);    //Beschleunigen
//        printf("Button 0: %i        \n",b0);    //A
//        printf("Button 1: %i        \n",b1);    //B
//        printf("Button 2: %i        \n",b2);    //X
//        printf("Button 3: %i        \n",b3);    //Y
//        printf("Button 4: %i        \n",b4);    //LB --> Disconnect
//        printf("Button 5: %i        \n",b5);    //RB
//        printf("Button 6: %i        \n",b6);    //ZweiKasten
//        printf("Button 7: %i        \n",b7);    //Liste ---> engine off
        printf("Sensetifity-Level: %i                               \n",sensLevel+1);
        printf("Armed: %i                                           \n",isarmed);
        if(b0==1)
        {
            if(a1<-32600)
            {
                //arm
                a1=-32768;
                a0=32768;
            }
        }
        if(b1==1)
        {
            if(a1<-32600)
            {
                //disarm
                a1=-32768;
                a0=-32768;
            }
        }

        if(b4 == 1 && b5 == 1)
        {
            keepRunning = false;
        }

        //Adjusting Joystick endpositions
        if(a0==32767) a0++;
        if(a2==32767) a2++;
        if(a3==32767) a3++;
        if(a5==32767) a5++;
        if(a1==-32767) a1--;
        if(a4==-32767) a4--;

        AdjustingValue = adjustingArray[sensLevel];
        JoystickData[0] = normalize_xbox(a3, AdjustingValue, 3);
        JoystickData[1] = normalize_xbox(a4, AdjustingValue, 4);
        JoystickData[2] = normalize_xbox(a0, AdjustingValue, 0);
        JoystickData[3] = normalize_xbox(a1, AdjustingValue, 1);    //a1
        JoystickData[4] = normalize_xbox(32768, AdjustingValue, 2);    //a2
        JoystickData[5] = normalize_xbox(a5, AdjustingValue, 5);
        JoystickData[6] = htons(armedValue);
        JoystickData[7] = htons(1000);

        sendPacket(JoystickData);
//            count++;
//            if(count == 55){
//                gettimeofday(&tv, NULL);
//
//                double end = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
//                printf("Dauer für 55 mal senden: %f ms",end-begin);
//                keepRunning = false;
//            }
    }
    //Used to disconnect all sockets on flight controller (Pi) and remote control
    unsigned short JoystickData1[] = {htons(1000),htons(1000),htons(1000),
                                      htons(1000),htons(1000),htons(1000),
                                      htons(1000),htons(6666)
                                     };
    sendPacket(JoystickData1);
    closeSocket();
    SDL_JoystickClose(joystick);
    return 0;
}

unsigned short normalize_xbox(int value, int adjustingValue, int axis)
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
    return htons(computed);
}
