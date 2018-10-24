cmake_minimum_required(VERSION 3.3)
project(osd)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /tmp)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux")

link_directories(/opt/vc/lib/)
add_subdirectory(../common db_common)

set(SOURCE_FILES_OSD
        main.c
        render.c
        render.h
        telemetry.c
        telemetry.h
        frsky.c
        frsky.h
        ltm.c
        osdconfig.h
        ltm.h
        osd_mavlink.c
        osd_mavlink.h
        smartport.c
        smartport.h)

add_executable(osd ${SOURCE_FILES_OSD})
target_link_libraries(osd db_common)

if(UNIX AND NOT APPLE)
    target_link_libraries(osd GLESv2 EGL openmaxil bcm_host vcos vchiq_arm pthread rt m shapes freetype z)
endif()