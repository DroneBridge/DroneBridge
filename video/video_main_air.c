/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Based on wifibroadcast tx by Befinitiv. Licensed under GPL2
 *   integrated into the DroneBridge extensions by Wolfgang Christl
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../common/shared_memory.h"
#include "../common/db_protocol.h"
#include "../common/db_raw_receive.h"

bool volatile keeprunning = true;
void intHandler(int dummy)
{
    keeprunning = false;
}

int process_command_line_args(int argc, char *argv[]){
    while ((c = getopt (argc, argv, "n:m:c:p:")) != -1)
    {

    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, intHandler);
    usleep((__useconds_t) 1e6);
    process_command_line_args(argc, argv);

}
