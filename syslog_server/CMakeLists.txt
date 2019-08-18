cmake_minimum_required(VERSION 3.3)
project(syslog_server)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

add_subdirectory(../common db_common)
set(SOURCE_FILES syslog_server_main.c)

add_executable(db_syslog_server ${SOURCE_FILES})
target_link_libraries(db_syslog_server db_common)