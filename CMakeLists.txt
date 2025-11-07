cmake_minimum_required(VERSION 3.10)
project(webcam_streamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(webcam_streamer
    main.cpp
)

target_include_directories(webcam_streamer PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(webcam_streamer PRIVATE ${OpenCV_LIBS})
