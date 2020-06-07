# Install script for directory: /home/cyber/repositories/DroneBridge/common

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release;...;FORCE")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/DroneBridge" TYPE STATIC_LIBRARY FILES "/home/cyber/repositories/DroneBridge/common/libdb_common.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/DroneBridge" TYPE FILE FILES
    "/home/cyber/repositories/DroneBridge/common/db_common.h"
    "/home/cyber/repositories/DroneBridge/common/db_protocol.h"
    "/home/cyber/repositories/DroneBridge/common/db_raw_receive.h"
    "/home/cyber/repositories/DroneBridge/common/db_crc.h"
    "/home/cyber/repositories/DroneBridge/common/shared_memory.h"
    "/home/cyber/repositories/DroneBridge/common/msp_serial.h"
    "/home/cyber/repositories/DroneBridge/common/db_utils.h"
    "/home/cyber/repositories/DroneBridge/common/tcp_server.h"
    "/home/cyber/repositories/DroneBridge/common/db_unix.h"
    "/home/cyber/repositories/DroneBridge/common/radiotap/platform.h"
    "/home/cyber/repositories/DroneBridge/common/radiotap/radiotap.h"
    "/home/cyber/repositories/DroneBridge/common/radiotap/radiotap_iter.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/cyber/repositories/DroneBridge/common/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
