cmake_minimum_required(VERSION 3.10)
project(WebcamStreamer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0)

include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})

add_executable(webcam_stream main.cpp)
target_link_libraries(webcam_stream ${GST_LIBRARIES})
