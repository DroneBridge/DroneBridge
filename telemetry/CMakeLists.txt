cmake_minimum_required(VERSION 3.5)
project(telemetry)

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
set(SOURCE_FILES telemetry_main_air.c telemetry_main_air.h)

add_executable(telemetry_air ${SOURCE_FILES})
target_link_libraries(telemetry_air db_common)