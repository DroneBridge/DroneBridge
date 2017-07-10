cmake_minimum_required(VERSION 2.5)
project(control_tx)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        db_protocol.h
        i6S.c
        main.c
        main.h
        parameter.h
        tx.c)

add_executable(control_tx ${SOURCE_FILES})
