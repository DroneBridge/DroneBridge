cmake_minimum_required(VERSION 3.5)
project(status)

set(CMAKE_C_STANDARD 11)
set(SOURCE_FILES main.c lib.h wifibroadcast.h)
add_executable(status ${SOURCE_FILES})
if(UNIX AND NOT APPLE)
    target_link_libraries(status rt)
endif()
