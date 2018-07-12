cmake_minimum_required(VERSION 3.3)
project(control)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

add_subdirectory(../common db_common)

set(SOURCE_FILES_CONTROL_GROUND
        i6S.c
        control_main_ground.c
        parameter.h
        rc_ground.h
        i6S.h rc_ground.c opentx.c opentx.h)

set(SOURCE_FILES_CONTROL_AIR
        control_main_air.c rc_air.c rc_air.h)

set(SOURCE_FILES_CONTROL_SUMDTEST
        sumd_test.c)

add_executable(control_ground ${SOURCE_FILES_CONTROL_GROUND})
target_link_libraries(control_ground db_common)

add_executable(control_air ${SOURCE_FILES_CONTROL_AIR})
target_link_libraries(control_air db_common)

add_executable(sumd_test ${SOURCE_FILES_CONTROL_SUMDTEST})
target_link_libraries(sumd_test db_common)