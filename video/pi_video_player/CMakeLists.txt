cmake_minimum_required(VERSION 3.3)
project(pi_video_player)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O3")        ## Optimize

link_directories(/opt/vc/lib/)

include_directories (/opt/vc/include /opt/vc/src/hello_pi/libs/ilclient)

set(SOURCE_PLAYER video.c.player.c)
set(SOURCE_PLAYER_30 video.c.30.c)
set(SOURCE_PLAYER_48 video.c.48.c)
set(SOURCE_PLAYER_240 video.c.240.c)

add_executable(hello_video.bin.player ${SOURCE_PLAYER})
add_executable(hello_video.bin.30 ${SOURCE_PLAYER_30})
add_executable(hello_video.bin.48 ${SOURCE_PLAYER_48})
add_executable(hello_video.bin.240 ${SOURCE_PLAYER_240})

if(UNIX AND NOT APPLE)
    target_link_libraries(hello_video.bin.player brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(hello_video.bin.30 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(hello_video.bin.48 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    target_link_libraries(hello_video.bin.240 brcmGLESv2 brcmEGL openmaxil bcm_host vcos vchiq_arm pthread ilclient)
    # target_link_libraries(osd GLESv2 EGL openmaxil bcm_host vcos vchiq_arm pthread rt m shapes freetype z)
endif()