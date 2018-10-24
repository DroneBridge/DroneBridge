cmake_minimum_required(VERSION 3.3)
project(dronebridge)

add_subdirectory(control)
add_subdirectory(status)
add_subdirectory(proxy)
add_subdirectory(telemetry)
add_subdirectory(video)
if (EXISTS "/opt/vc/include/bcm_host.h")
    # only build on Raspberry Pi
    add_subdirectory(splash)
    add_subdirectory(osd)
endif ()
add_subdirectory(common)
