cmake_minimum_required(VERSION 2.8)
project(db_esp32)

include_directories($IDF_PATH)

add_executable(db_esp32 main/main.c main/db_esp32_control.c)