cmake_minimum_required(VERSION 3.3)
project(common)

if(TARGET common_lib)

    message("common_lib is already defined")

else()

    include_directories(${SHARED_LIB_INCLUDE_DIR})

    set(LIB_SRCS
            db_protocol.h
            db_get_ip.c
            db_get_ip.h
            db_raw_receive.c
            db_raw_receive.h
            db_raw_send.c
            wifibroadcast.h
            lib.h)

    add_library(common_lib STATIC ${LIB_SRCS})

endif()