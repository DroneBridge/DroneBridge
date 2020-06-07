cmake_minimum_required(VERSION 3.5)
project(video)

set(CMAKE_C_STANDARD 11)

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release ... FORCE)
ENDIF ()

IF (CMAKE_BUILD_TYPE MATCHES Release)
    SET(CMAKE_C_FLAGS "-O3") ## Optimize
    message(STATUS "${PROJECT_NAME} module: Release configuration")
ELSE ()
    message(STATUS "${PROJECT_NAME} module: Debug configuration")
ENDIF ()

add_subdirectory(../common db_common)
set(SOURCE_FILES_GND
        video_main_gnd.c fec.c fec.h video_lib.c video_lib.h)

set(SOURCE_FILES_AIR 
        video_main_air.c fec.c fec.h video_lib.c video_lib.h)

add_executable(video_gnd ${SOURCE_FILES_GND})
target_link_libraries(video_gnd db_common)

add_executable(video_air ${SOURCE_FILES_AIR})
target_link_libraries(video_air db_common)
