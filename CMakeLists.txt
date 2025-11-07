cmake_minimum_required(VERSION 3.10)
project(HttpWebcamStream)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0 gstreamer-webm-1.0)

include_directories(${GST_INCLUDE_DIRS})
link_directories(${GST_LIBRARY_DIRS})

add_executable(http_webcam_stream http_webcam_stream.cpp)
target_link_libraries(http_webcam_stream ${GST_LIBRARIES})
