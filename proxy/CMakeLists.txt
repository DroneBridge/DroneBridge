cmake_minimum_required(VERSION 3.5)
project(proxy_ground)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

add_subdirectory(../common common_lib)
set(SOURCE_FILES proxy_main.c)

add_executable(proxy ${SOURCE_FILES})
target_link_libraries(proxy common_lib)
