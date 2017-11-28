cmake_minimum_required(VERSION 2.5)
project(control_ground)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

set(SOURCE_FILES
        db_protocol.h
        i6S.c
        main.c
        main.h
        parameter.h
        tx.c)

add_executable(control_ground ${SOURCE_FILES})
