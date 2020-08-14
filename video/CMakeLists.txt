cmake_minimum_required(VERSION 3.5)
project(video)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release ... FORCE)
ENDIF ()

IF (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
    message(STATUS "\tVideo Module: Compiling for ARM - ${CMAKE_SYSTEM_PROCESSOR}")
    IF (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv6")
        message(STATUS "\tVideo Module: Setting NEON flags")
        SET(CMAKE_C_FLAGS "-mfpu=neon -march=native ${CMAKE_C_FLAGS}")
        SET(CMAKE_CXX_FLAGS "-mfpu=neon -march=native ${CMAKE_CXX_FLAGS}")
    ENDIF()
    ADD_DEFINITIONS(-DLINUX_ARM)
ELSE()
    message(STATUS "\tVideo Module: Compiling for SSEX")
    SET(CMAKE_C_FLAGS "-msse3 -msse4.1 -mavx2 ${CMAKE_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS "-msse3 -msse4.1 -mavx2 ${CMAKE_CXX_FLAGS}")
ENDIF()


IF (CMAKE_BUILD_TYPE MATCHES Release)
    SET(CMAKE_C_FLAGS "-O3 ${CMAKE_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
    ADD_DEFINITIONS(-DO3Enabled)
    message(STATUS "${PROJECT_NAME} module: Release configuration")
ELSE ()
    message(STATUS "${PROJECT_NAME} module: Debug configuration")
ENDIF ()

add_subdirectory(../common db_common)
set(SOURCE_FILES_GND
        video_main_gnd.c fec.c fec.h video_lib.c video_lib.h)

set(SOURCE_FILES_AIR 
        video_main_air.c fec.c fec.h video_lib.c video_lib.h)

set(GF256_LIB_SRCFILES
        gf256.cpp
        gf256.h)

set(SOURCE_FILES_SPEEDTEST
        fec_speed_test.c fec_speed_test.h fec_old.h fec_old.c fec.c fec.h)

add_library(gf256 ${GF256_LIB_SRCFILES})

add_executable(video_gnd ${SOURCE_FILES_GND})
target_link_libraries(video_gnd db_common gf256)

add_executable(video_air ${SOURCE_FILES_AIR})
target_link_libraries(video_air db_common gf256)

add_executable(fec_speed_test ${SOURCE_FILES_SPEEDTEST})
target_link_libraries(fec_speed_test gf256)
