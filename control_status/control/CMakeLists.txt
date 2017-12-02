cmake_minimum_required(VERSION 3.3)
project(control)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

add_subdirectory(../common common_lib)

set(SOURCE_FILES_CONTROL_GROUND
        i6S.c
        control_main_ground.c
        parameter.h
        tx.h
        i6S.h tx.c)

set(SOURCE_FILES_CONTROL_AIR
        control_main_air.c)

add_executable(control_ground ${SOURCE_FILES_CONTROL_GROUND})
target_link_libraries(control_ground common_lib)

add_executable(control_air ${SOURCE_FILES_CONTROL_AIR})
target_link_libraries(control_air common_lib)