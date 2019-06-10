cmake_minimum_required(VERSION 3.5)
project(video_wbc_legacy)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

set(SOURCE_FILES_TX_MEASURE
        tx_measure.c wifibroadcast.h fec.c fec.h lib.h lib.c)

add_executable(tx_measure ${SOURCE_FILES_TX_MEASURE})
# target_link_libraries(video_gnd db_common)