cmake_minimum_required(VERSION 3.5)
project(proxy_ground)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

add_subdirectory(../common db_common)
set(SOURCE_FILES proxy_main.c proxy_main.h)

add_executable(db_proxy ${SOURCE_FILES})
target_link_libraries(db_proxy db_common)
