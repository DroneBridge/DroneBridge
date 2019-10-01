cmake_minimum_required(VERSION 3.5)
project(injection_test)

set(CMAKE_C_STANDARD 11)

add_subdirectory(../common db_common)
set(SOURCE_FILES main.c main.h)
set(SRC_FILES_RECV receive_main.c)
set(SRC_FILES_SEND send_main.c)

add_executable(it ${SOURCE_FILES})
target_link_libraries(it db_common)

add_executable(receive ${SRC_FILES_RECV})
target_link_libraries(receive db_common)

add_executable(send ${SRC_FILES_SEND})
target_link_libraries(send db_common)