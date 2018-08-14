cmake_minimum_required(VERSION 3.3)
project(db_common)

if(TARGET db_common)

    message("db_common is already defined")

else()

    include_directories(${SHARED_LIB_INCLUDE_DIR})

    set(LIB_SRCS
            db_protocol.h
            db_get_ip.c
            db_get_ip.h
            db_raw_receive.c
            db_raw_receive.h db_raw_send_receive.c wifibroadcast.h wbc_lib.h db_crc.h shared_memory.c shared_memory.h
            msp_serial.c msp_serial.h db_crc.c ccolors.h db_utils.c db_utils.h
            mavlink)

    add_library(db_common STATIC ${LIB_SRCS})


    if(UNIX AND NOT APPLE)
        target_link_libraries(db_common rt)
    endif()

endif()