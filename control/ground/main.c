#include <stdio.h>
#include <stdlib.h>
//#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "parameter.h"

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

int main(int argc, char *argv[]) {
    atexit(closeSocket);
    char ifName[IFNAMSIZ];
    char calibrate_comm[500];
    int Joy_IF, c, bitrate_op;
    uint8_t mac[6];
    char macStr[18];
    char ab_mode = 'm';

    // Command Line processing
    strcpy(macStr, DEST_MAC_CHAR);
    Joy_IF = JOY_INTERFACE;
    bitrate_op = DEFAULT_BITRATE_OPTION;
    strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
    strcpy(ifName, DEFAULT_IF);
    opterr = 0;
    while ((c = getopt(argc, argv, "n:j:d:m:b:c:")) != -1) {
        switch (c) {
            case 'n':
                strcpy(ifName, optarg);
                break;
            case 'j':
                Joy_IF = (int) strtol(optarg, NULL, 10);
                break;
            case 'd':
                strcpy(macStr, optarg);
                break;
            case 'm':
                ab_mode = *optarg;
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'c':
                strcpy(calibrate_comm, optarg);
                break;
            case '?':
                printf("Use following commandline arguments.\n");
                printf("-n network interface for long range \n-j number of joystick interface of RC \n"
                               "-d MAC address of RX interface on drone \n-m mode: <w|m> for wifi or monitor\n-c a "
                               "command to calibrate the joystick. Gets executed on initialisation \n"
                               "-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n");
                return -1;
            default:
                abort();
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

    if (openSocket(ifName, mac, ab_mode, bitrate_op) > 0) {
        printf("Could not open socket");
        return -1;
    }
    //if(determineController(joystick)==0){
    printf("Choosing i6S-Config\n");
    i6S(Joy_IF, calibrate_comm);
    //}else{
    //    SDL_Joystick *joystick;
    //    joystick = SDL_JoystickOpen(Joy_IF);
    //    printf("Choosing XBOX-Config");
    //    xbox_only(joystick);
    //}

    return 0;
}


