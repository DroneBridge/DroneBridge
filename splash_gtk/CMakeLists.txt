cmake_minimum_required(VERSION 3.3)
project(splash_gtk)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES_SPLASH
        splash_gtk.c)

find_package(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK REQUIRED gtk+-3.0)
INCLUDE_DIRECTORIES(${GTK_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK_LIBRARY_DIRS})

# Add other flags to the compiler
ADD_DEFINITIONS(${GTK_CFLAGS_OTHER})

add_executable(splash_gtk ${SOURCE_FILES_SPLASH})
target_link_libraries(splash_gtk ${GTK_LIBRARIES})

file(COPY glade DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
