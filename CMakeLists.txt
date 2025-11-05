cmake_minimum_required(VERSION 3.10)
project(webcam_streamer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect platform
if(WIN32)
    # Windows - specify GStreamer paths manually
    set(GSTREAMER_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/include/gstreamer-1.0")
    set(GLIB_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/include/glib-2.0")
    set(GLIB_LIB_INCLUDE_DIR "C:/gstreamer/1.0/x86_64/lib/glib-2.0/include")
    set(GSTREAMER_LIB_DIR "C:/gstreamer/1.0/x86_64/lib")

    find_package(Boost 1.65 REQUIRED COMPONENTS system filesystem)

    add_executable(webrtc_server main.cpp)

    target_include_directories(webrtc_server PRIVATE
        ${GSTREAMER_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIR}
        ${GLIB_LIB_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS}
    )

    link_directories(${GSTREAMER_LIB_DIR})

    target_link_libraries(webrtc_server
        gstreamer-1.0
        gstwebrtc-1.0
        gstsdp-1.0
        ${Boost_LIBRARIES}
    )
else()
    # Linux / WSL - use pkg-config to find GStreamer
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0)

    find_package(Boost 1.65 REQUIRED COMPONENTS system filesystem)

    add_executable(webrtc_server main.cpp)

    target_include_directories(webrtc_server PRIVATE
        ${GST_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    )

    target_link_libraries(webrtc_server
        ${GST_LIBRARIES}
        ${Boost_LIBRARIES}
    )

    target_compile_options(webrtc_server PRIVATE ${GST_CFLAGS_OTHER})
endif()
