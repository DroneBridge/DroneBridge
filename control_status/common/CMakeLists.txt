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
            db_raw_receive.h db_raw_send_receive.c wifibroadcast.h lib.h db_rc_crc.h shared_memory.c shared_memory.h
            mavlink_v2)


    add_library(common_lib STATIC ${LIB_SRCS})
    if(UNIX AND NOT APPLE)
        target_link_libraries(common_lib rt)
    endif()

endif()