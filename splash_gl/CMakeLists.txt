cmake_minimum_required(VERSION 3.3)
project(splash_gl)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES_SPLASH
        splash_nanovg.c)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

add_executable(splash_gl ${SOURCE_FILES_SPLASH})
target_link_libraries(splash_gl nanovg)
target_link_libraries(splash_gl ${GLFW_STATIC_LIBRARIES})
