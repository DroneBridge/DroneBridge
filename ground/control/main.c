#include <stdio.h>
#include <stdlib.h>
//#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "parameter.h"

struct crcdata *thecrcdata;

/*int determineController(SDL_Joystick *joystick){
    printf("detected: %s\n", SDL_JoystickName(0));
    if (strcmp(SDL_JoystickName(0), MY_RC)==0){
        // Found Turnigy/Flysky i6S RC
        return 0;
    }else{
        //found Xbox gamepad or something else
        return 1;
    }
}*/

int main(int argc, char *argv[])
{
    atexit (closeSocket);
    char ifName[IFNAMSIZ];
    int Joy_IF, c;
    uint8_t mac[6];
    char macStr[18];
    char ab_mode = 'm';
    strcpy(macStr,DEST_MAC_CHAR);


    // Command Line processing
    Joy_IF = JOY_INTERFACE;
    strcpy(ifName, DEFAULT_IF);
    opterr = 0;
    while ((c = getopt (argc, argv, "n:j:d:m:")) != -1)
    {
        switch (c)
        {
        case 'n':
            strcpy(ifName, optarg);
            break;
        case 'j':
            Joy_IF = (int) strtol(optarg, NULL, 0);
            break;
        case 'd':
            strcpy(macStr, optarg);
            break;
        case 'm':
            ab_mode = *optarg;
            break;
        case '?':
            printf("There was a problem with commandline arguments.\n");
            printf("Use -n <network_IF> -j <joystick_Interface_for_controller> -d <MAC_of_RX_Interface_ON_DRONE> -m <w|m>\n");
            break;
        default:
            abort ();
        }
    }
    sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);


    // TODO: clean code
    // This part below is only needed when using xbox controller ... some cleaning is required
//    if (SDL_Init( SDL_INIT_JOYSTICK ) < 0)
//    {
//        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
//        exit(1);
//    }
//    printf("Using SDL-Version:%i\n", SDL_COMPILEDVERSION);
//    while(SDL_NumJoysticks() == 0){
//        SDL_Init( SDL_INIT_JOYSTICK );
//        printf("No joysticks found. Please connect! Waiting...\n");
//        sleep(1);
//    }
//    if (SDL_NumJoysticks() == 0)
//    {
//        printf("No Gamepad found :\( \n");
//        exit(1);
//    }
    //else
    //{
        //printf("%d joysticks found, choosing #%i for controlling the drone\n", SDL_NumJoysticks(), Joy_IF);
    //}

    if(openSocket(ifName, mac, ab_mode)>0)
    {
        printf("Could not open socket");
        return 0;
    }
    //if(determineController(joystick)==0){
    printf("Choosing i6S-Config\n");
    i6S(Joy_IF);
    //}else{
    //    SDL_Joystick *joystick;
    //    joystick = SDL_JoystickOpen(Joy_IF);
    //    printf("Choosing XBOX-Config");
    //    xbox_only(joystick);
    //}

    return 0;
}


