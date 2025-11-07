cmake_minimum_required(VERSION 3.10)

project(WebcamStreamServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV package for webcam/video capture and image encoding
find_package(OpenCV REQUIRED)

# Find Boost libraries (header-only for Asio or you can link Boost.Asio explicitly)
find_package(Boost REQUIRED COMPONENTS system thread)

include_directories(${OpenCV_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

add_executable(webcam_stream src/main.cpp)

target_link_libraries(webcam_stream ${OpenCV_LIBS} ${Boost_LIBRARIES})

