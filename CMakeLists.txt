cmake_minimum_required(VERSION 3.10)

project(webcam_streamer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Specify GStreamer installation paths - adjust these paths accordingly
set(GSTREAMER_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/include/gstreamer-1.0")
set(GLIB_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/include/glib-2.0")
set(GLIB_LIB_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/lib/glib-2.0/include")

set(GSTREAMER_LIB_DIR "C:/gstreamer/1.0/x86_64/lib")

# Boost - find package normally
find_package(Boost 1.65 REQUIRED COMPONENTS system filesystem)

# Add executable
add_executable(webrtc_server main.cpp)

# Include directories
target_include_directories(webrtc_server PRIVATE 
    ${GSTREAMER_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIR}
    ${GLIB_LIB_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# Link directories for GStreamer
link_directories(${GSTREAMER_LIB_DIR})

# Link libraries
target_link_libraries(webrtc_server 
    gstreamer-1.0
    gstwebrtc-1.0
    gstsdp-1.0
    ${Boost_LIBRARIES}
)

# Compile options can be added manually if needed
