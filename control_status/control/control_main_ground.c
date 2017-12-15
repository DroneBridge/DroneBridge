//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include "parameter.h"
#include "../common/db_protocol.h"
#include "i6S.h"
#include "../common/db_raw_send_receive.h"
#include "rc_ground.h"

int detect_RC(int new_Joy_IF) {
    int fd;
    char interface_joystick[500];
    char path[] = "/dev/input/js";
    sprintf(interface_joystick, "%s%d", path, new_Joy_IF);
    printf("DB_CONTROL_GROUND: Waiting for a RC to be detected on: %s\n", interface_joystick);
    do {
        usleep(250);
        fd = open(interface_joystick, O_RDONLY | O_NONBLOCK);
    } while (fd < 0);
    return fd;
}

int main(int argc, char *argv[]) {
    atexit(close_socket_send_receive);
    char ifName[IFNAMSIZ], RC_name[128];
    char calibrate_comm[500];
    uint8_t comm_id;
    int Joy_IF, c, bitrate_op, rc_protocol;
    char db_mode = 'm';

    // Command Line processing
    Joy_IF = JOY_INTERFACE;
    rc_protocol = 5;
    bitrate_op = DEFAULT_BITRATE_OPTION;
    comm_id = DEFAULT_V2_COMMID;
    strcpy(calibrate_comm, DEFAULT_i6S_CALIBRATION);
    strcpy(ifName, DEFAULT_IF);
    opterr = 0;
    while ((c = getopt(argc, argv, "n:j:m:b:g:v:c:")) != -1) {
        switch (c) {
            case 'n':
                strncpy(ifName, optarg, IFNAMSIZ);
                break;
            case 'j':
                Joy_IF = (int) strtol(optarg, NULL, 10);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case 'g':
                strcpy(calibrate_comm, optarg);
                break;
            case 'v':
                rc_protocol = (int) strtol(optarg, NULL, 10);
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("Use following commandline arguments.\n");
                printf("-n network interface for long range \n"
                               "-j number of joystick interface of RC \n"
                               "-m mode: <w|m> for wifi or monitor\n"
                               "-g a command to calibrate the joystick. Gets executed on initialisation\n"
                               "-v Protocol [1|2|5]: 1 = MSPv1 [Betaflight/Cleanflight]; 2 = MSPv2 [iNAV]; "
                               "3 = MAVLink (unsupported); 4 = MAVLink v2 (unsupported); 5 = DB-RC (default)\n"
                               "-c <communication id> Choose a number from 0-255. Same on groundstation and drone!\n"
                               "-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n(bitrate option only supported with Ralink chipsets)\n");
                return -1;
            default:
                abort();
        }
    }

    if (open_socket_send_receive(ifName, comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_CONTROLLER) < 0) {
        printf("DB_CONTROL_GROUND: Could not open socket\n");
        exit(-1);
    }
    conf_rc_protocol(rc_protocol);
    open_rc_tx_shm();

    int sock_fd = detect_RC(Joy_IF);
    if (ioctl(sock_fd, JSIOCGNAME(sizeof(RC_name)), RC_name) < 0)
        strncpy(RC_name, "Unknown", sizeof(RC_name));
    close(sock_fd); // We reopen in the RC specific file. Only opened in here to get the name of the controller
    if (strcmp(i6S_descriptor, RC_name) == 0){
        printf("DB_CONTROL_GROUND: Choosing i6S-Config\n");
        i6S(Joy_IF, calibrate_comm);
    } else {
        printf("DB_CONTROL_GROUND: Your RC \"%s\" is currently not supported. Closing.\n", RC_name);
    }
    return 0;
}