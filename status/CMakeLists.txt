cmake_minimum_required(VERSION 3.5)
project(status_ground)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

add_subdirectory(../common common_lib)
set(SOURCE_FILES status_main.c)

add_executable(status ${SOURCE_FILES})
target_link_libraries(status common_lib)