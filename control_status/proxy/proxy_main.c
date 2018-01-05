/*
 *   Created by Wolfgang Christl
 *   This file is part of DroneBridge
 *   https://github.com/seeul8er/DroneBridge
 *   This is the DroneBridge Proxy module. It routes UDP <-> DroneBridge Control module and is used to send MSP/MAVLink
 *   messages
 *   This module might act as a reference design for future modules
 *   Link over DroneBridge Proxy module is fully transparent
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zconf.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include "../common/lib.h"
#include "../common/db_get_ip.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"
#include "../common/db_raw_send_receive.h"

bool volatile keeprunning = true;
char if_name[IFNAMSIZ];
char db_mode;
uint8_t comm_id = DEFAULT_V2_COMMID;
int c, app_port_proxy = APP_PORT_PROXY, bitrate_op;

void intHandler(int dummy)
{
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]){
    strncpy(if_name, DEFAULT_DB_IF, IFNAMSIZ);
    db_mode = DEFAULT_DB_MODE;
    app_port_proxy = APP_PORT_PROXY;
    opterr = 0;
    bitrate_op = DEFAULT_BITRATE_OPTION;
    while ((c = getopt (argc, argv, "n:m:c:p:b:")) != -1)
    {
        switch (c)
        {
            case 'n':
                strncpy(if_name, optarg, IFNAMSIZ);
                break;
            case 'm':
                db_mode = *optarg;
                break;
            case 'c':
                comm_id = (uint8_t) strtol(optarg, NULL, 10);
                break;
            case 'p':
                app_port_proxy = (int) strtol(optarg, NULL, 10);
                break;
            case 'b':
                bitrate_op = (int) strtol(optarg, NULL, 10);
                break;
            case '?':
                printf("DroneBridge Proxy module is used to do any UDP <-> DB_CONTROL_AIR routing. UDP IP given by "
                               "IP-checker module. Use"
                               "\n\t-n <network_IF> "
                               "\n\t-m [w|m] default is <m>"
                               "\n\t-p Specify a UDP port to which we send the data received over long range link. IP "
                               "comes from IP checker module. This port is also the local port for receiving UDP packets"
                               " to forward to DB_raw. Default port:%i"
                               "\n\t-c <communication id> Choose a number from 0-255. Same on groundstation and drone!"
                               "\n\t-b bitrate: \n\t1 = 2.5Mbit\n\t2 = 4.5Mbit\n\t3 = 6Mbit\n\t4 = 12Mbit (default)\n\t"
                               "5 = 18Mbit\n\t(bitrate option only supported with Ralink chipsets)"
                        , APP_PORT_PROXY);
                break;
            default:
                abort ();
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    process_command_line_args(argc, argv);

    // set up long range receiving socket
    int long_range_socket = open_socket_send_receive(if_name, comm_id, db_mode, bitrate_op, DB_DIREC_DRONE, DB_PORT_PROXY);
    int udp_socket = socket (AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in remoteServAddr, servAddr;
    // set up UDP socket remote address
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_addr.s_addr = inet_addr("192.168.2.2");
    remoteServAddr.sin_port = htons(app_port_proxy);

    // local server port we bind to
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(app_port_proxy);
    const int y = 1;
    setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));

    if (udp_socket < 0) {
        printf ("DB_PROXY_GROUND: Unable to open socket (%s)\n", strerror(errno));
        exit (EXIT_FAILURE);
    }
    if (bind(udp_socket, (struct sockaddr *) &servAddr, sizeof (servAddr)) < 0) {
        printf ("DB_PROXY_GROUND: Unable to bind to port %i (%s)\n", app_port_proxy, strerror(errno));
        exit (EXIT_FAILURE);
    }
    int broadcast=1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast,sizeof(broadcast))==-1) {
        printf("DB_PROXY_GROUND: Unable to set broadcast option %s\n",strerror(errno));
    }

    // init variables
    int shID = init_shared_memory_ip();
    fd_set fd_socket_set;
    struct timeval select_timeout;
    struct data_uni *data_uni_to_drone = (struct data_uni *)
            (monitor_framebuffer + RADIOTAP_LENGTH + DB_RAW_V2_HEADER_LENGTH);
    uint8_t seq_num = 0;
    uint8_t lr_buffer[DATA_UNI_LENGTH];
    uint8_t udp_buffer[DATA_UNI_LENGTH-DB_RAW_V2_HEADER_LENGTH];

    printf("DB_PROXY_GROUND: started!\n");
    while(keeprunning) {
        select_timeout.tv_sec = 2;
        select_timeout.tv_usec = 0;
        FD_ZERO (&fd_socket_set);
        FD_SET (udp_socket, &fd_socket_set);
        FD_SET (long_range_socket, &fd_socket_set);
        int select_return = select (FD_SETSIZE, &fd_socket_set, NULL, NULL, &select_timeout);
        if(select_return == 0){
            // timeout
            remoteServAddr.sin_addr.s_addr = inet_addr(get_ip_from_ipchecker(shID));
        } else if (select_return > 0){
            if (FD_ISSET(udp_socket, &fd_socket_set)){
                // ---------------
                // Message app/UDP --> DB_CONTROL_AIR
                // ---------------
                ssize_t l = recv(udp_socket, udp_buffer, (DATA_UNI_LENGTH-DB_RAW_V2_HEADER_LENGTH), 0);
                int err = errno;
                if (l > 0){
                    memcpy(data_uni_to_drone->bytes, udp_buffer, (size_t) l);
                    send_packet_hp(DB_PORT_CONTROLLER, (u_int16_t) l, update_seq_num(&seq_num));
                } else {
                    printf("DB_PROXY_GROUND: UDP socket received an error: %s\n", strerror(err));
                }
            }
            if (FD_ISSET(long_range_socket, &fd_socket_set)){
                // ---------------
                // Message DB_CONTROL_AIR --> app/UDP
                // ---------------
                ssize_t l = recv(long_range_socket, lr_buffer, DATA_UNI_LENGTH, 0); int err = errno;
                if (l > 0){
                    int radiotap_length = lr_buffer[2] | (lr_buffer[3] << 8);
                    size_t message_length = lr_buffer[radiotap_length+7] | (lr_buffer[radiotap_length+8] << 8); // DB_v2
                    memcpy(udp_buffer, lr_buffer+(radiotap_length + DB_RAW_V2_HEADER_LENGTH), message_length);
                    sendto (udp_socket, udp_buffer, message_length, 0, (struct sockaddr *) &remoteServAddr,
                            sizeof (remoteServAddr));
                } else {
                    printf("DB_PROXY_GROUND: Long range socket received an error: %s\n", strerror(err));
                }
            }
        } else if (select_return == -1) {
            perror("DB_PROXY_GROUND: select() returned error: ");
        }
    }
    close(long_range_socket);
    close(udp_socket);
    return 0;
}