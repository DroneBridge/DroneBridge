cmake_minimum_required(VERSION 3.5)
project(video_wbc_legacy)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3") ## Optimize

include(FindPCAP.cmake)

set(SOURCE_FILES_TX_MEASURE
        tx_measure.c wifibroadcast.h fec.c fec.h lib.h lib.c)

set(SOURCE_FILES_TX_LEGACY
        tx_rawsock.c ieee80211_radiotap.h wifibroadcast.h fec.h fec.c lib.c lib.h radiotap.h radiotap.c)

set(SOURCE_FILES_RX_LEGACY
        rx.c ieee80211_radiotap.h wifibroadcast.h fec.h fec.c lib.c lib.h radiotap.h radiotap.c)

add_executable(tx_measure ${SOURCE_FILES_TX_MEASURE})
add_executable(tx_vid ${SOURCE_FILES_TX_LEGACY})
add_executable(rx_vid ${SOURCE_FILES_RX_LEGACY})
target_link_libraries(rx_vid ${PCAP_LIBRARY})
if(UNIX AND NOT APPLE)
    target_link_libraries(tx_vid rt)
    target_link_libraries(rx_vid rt)
endif()