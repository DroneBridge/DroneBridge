cmake_minimum_required(VERSION 2.8)
project(db_esp32)

add_executable(db_esp32 main/main.c main/db_esp32_control.c)

target_include_directories(db_esp32 PRIVATE $IDF_PATH/home/cyber/Dokumente/Programming/esp32_SDK/esp/esp-idf/components/log/include)
