cmake_minimum_required(VERSION 2.5)
project(control_rx)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        db_protocol.h
        main.c)

add_executable(control_rx ${SOURCE_FILES})
