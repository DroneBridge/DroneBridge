cmake_minimum_required(VERSION 3.3)
project(usbbridge)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

find_package(PkgConfig)
pkg_check_modules(libusb-1.0 REQUIRED libusb-1.0)

add_subdirectory(../common db_common)
set(SOURCE_FILES usbbridge_main.c linux_aoa.h linux_aoa.c db_usb.h)

add_executable(usbbridge ${SOURCE_FILES})
target_link_libraries(usbbridge db_common ${libusb-1.0_LIBRARIES})