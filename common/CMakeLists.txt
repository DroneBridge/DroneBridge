cmake_minimum_required(VERSION 3.3)
project(db_common)

IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release ... FORCE)
ENDIF ()

IF (CMAKE_BUILD_TYPE MATCHES Release)
    SET(CMAKE_C_FLAGS "-O3") ## Optimize
    message(STATUS "${PROJECT_NAME} module: Release configuration")
ELSE ()
    message(STATUS "${PROJECT_NAME} module: Debug configuration")
ENDIF ()

if (TARGET db_common)
else ()
    include_directories(${SHARED_LIB_INCLUDE_DIR})

    set(LIB_SRCS
            db_raw_receive.c
            db_raw_send_receive.c
            shared_memory.c
            msp_serial.c db_crc.c db_utils.c
            mavlink
            radiotap/parse.c
            radiotap/radiotap.c tcp_server.c  db_unix.c)
    set(LIB_HEADERS
            db_common.h db_protocol.h db_raw_receive.h db_crc.h shared_memory.h msp_serial.h db_utils.h tcp_server.h
            db_unix.h
            radiotap/platform.h radiotap/radiotap.h radiotap/radiotap_iter.h)

    add_library(db_common STATIC ${LIB_SRCS} ${LIB_HEADERS})

    if (UNIX AND NOT APPLE)
        target_link_libraries(db_common rt)
        install(TARGETS db_common DESTINATION lib/DroneBridge)
        install(FILES ${LIB_HEADERS} DESTINATION include/DroneBridge)
    endif ()
endif ()