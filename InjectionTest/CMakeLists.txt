cmake_minimum_required(VERSION 3.5)
project(injection_test)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

add_subdirectory(../common db_common)
set(SOURCE_FILES main.c main.h)

add_executable(it ${SOURCE_FILES})
target_link_libraries(it db_common)