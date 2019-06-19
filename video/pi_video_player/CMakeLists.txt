cmake_minimum_required(VERSION 3.3)
project(pi_video_player)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

link_directories(/opt/vc/lib/)

include_directories (/opt/vc/include /opt/vc/src/hello_pi/libs/ilclient)

set(SOURCE_PLAYER video.c)
set(SOURCE_PLAYER_30 video_30.c)
set(SOURCE_PLAYER_48 video_48.c)
set(SOURCE_PLAYER_240 video_240.c)

add_executable(db_pi_player ${SOURCE_PLAYER})
add_executable(db_pi_player_30 ${SOURCE_PLAYER_30})
add_executable(db_pi_player_48 ${SOURCE_PLAYER_48})
add_executable(db_pi_player_240 ${SOURCE_PLAYER_240})

if(UNIX AND NOT APPLE)
    target_link_libraries(db_pi_player brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(db_pi_player_30 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(db_pi_player_48 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(db_pi_player_240 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    # target_link_libraries(osd GLESv2 EGL openmaxil bcm_host vcos vchiq_arm pthread rt m shapes freetype z)
endif()