cmake_minimum_required(VERSION 3.1)
project(control_air)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

set(SOURCE_FILES_CONTROL_AIR
        db_protocol.h
        control_main_air.c db_raw_send.c control_main_air.h)

add_executable(control_air ${SOURCE_FILES_CONTROL_AIR})
