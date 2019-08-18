cmake_minimum_required(VERSION 3.3)
project(syslog_server)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

set(SOURCE_FILES syslog_server_main.c)

add_executable(syslog_server ${SOURCE_FILES})