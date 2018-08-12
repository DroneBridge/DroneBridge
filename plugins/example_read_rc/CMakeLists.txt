cmake_minimum_required(VERSION 3.5)
project(readrc)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

set(SOURCE_FILES readrc.c db_protocol.h)

add_executable(readrc ${SOURCE_FILES})
# link libs for shm_open()
if(UNIX AND NOT APPLE)
    target_link_libraries(readrc rt)
endif()