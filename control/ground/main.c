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
    char comm_id[10];
    unsigned char comm_id_bytes[4];
    int Joy_IF, c, bitrate_op, frame_type;
    //char macStr[18];
    char ab_mode = 'm';

    // Command Line processing
    //strcpy(macStr, DEST_MAC_CHAR);
    Joy_IF = JOY_INTERFACE;
    frame_type = 1;
    bitrate_op = DEFAULT_BITRATE_OPTION;
    strcpy(comm_id, DEFAULT_COMMID);
    strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
    strcpy(ifName, DEFAULT_IF);
    opterr = 0;
    while ((c = getopt(argc, argv, "n:j:m:b:g:c:a:")) != -1) {
        switch (c) {
            case 'n':
                strncpy(ifName, optarg, IFNAMSIZ);
                break;
            case 'j':
                Joy_IF = (int) strtol(optarg, NULL, 10);
                break;
            case 'm':
                ab_mode = *optarg;
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'g':
                strcpy(calibrate_comm, optarg);
                break;
            case 'c':
                strncpy(comm_id, optarg, 10);
                break;
            case 'a':
                frame_type = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("Use following commandline arguments.\n");
                printf("-n network interface for long range \n-j number of joystick interface of RC \n"
                               "-m mode: <w|m> for wifi or monitor\n-g a command to calibrate the joystick."
                               " Gets executed on initialisation"
                               "\n-a frame type [1|2] <1> for Ralink und <2> for Atheros chipsets"
                               "\n-c the communication ID (same on TX and RX)\n"
                               "-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n(bitrate option only supported with Ralink chipsets)\n");
                return -1;
            default:
                abort();
        }
    }
    // sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    sscanf(comm_id, "%2hhx%2hhx%2hhx%2hhx", &comm_id_bytes[0], &comm_id_bytes[1], &comm_id_bytes[2], &comm_id_bytes[3]);
    printf("DB_CONTROL_TX: Interface: %s Communication ID: %02x %02x %02x %02x\n", ifName, comm_id_bytes[0], comm_id_bytes[1], comm_id_bytes[2], comm_id_bytes[3]);

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

    if (openSocket(ifName, comm_id_bytes, ab_mode, bitrate_op, frame_type) > 0) {
        printf("DB_CONTROL_TX: Could not open socket\n");
        return -1;
    }
    //if(determineController(joystick)==0){
    printf("DB_CONTROL_TX: Choosing i6S-Config\n");
    i6S(Joy_IF, calibrate_comm);
    //}else{
    //    SDL_Joystick *joystick;
    //    joystick = SDL_JoystickOpen(Joy_IF);
    //    printf("Choosing XBOX-Config");
    //    xbox_only(joystick);
    //}

    return 0;
}


