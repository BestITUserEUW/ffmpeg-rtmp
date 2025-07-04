cmake_minimum_required(VERSION 3.10...3.29)

project(ffmpeg-rtmp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(DEPS_BUILD_PATH "${PROJECT_BINARY_DIR}/thirdParty" CACHE PATH "Install path for the dependencies")
set(DEPS_INSTALL_PATH "${DEPS_BUILD_PATH}/install" CACHE PATH "Install path for the dependencies")

link_directories(
    ${DEPS_INSTALL_PATH}/lib
)

add_subdirectory(thirdParty)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenCV REQUIRED)
find_package(mimalloc REQUIRED)
find_package(oryx-crt-cpp REQUIRED)

pkg_search_module(x264 REQUIRED IMPORTED_TARGET x264)
pkg_search_module(libavcodec REQUIRED IMPORTED_TARGET libavcodec)
pkg_search_module(libswscale REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(libavutil REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(libavformat REQUIRED IMPORTED_TARGET libavformat)

add_library(common_runtime STATIC
    src/h264_encoder.cpp
    src/av_helpers.cpp
    src/rtmp_server.cpp
)

target_include_directories(common_runtime PUBLIC
    ${DEPS_INSTALL_PATH}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(common_runtime PUBLIC
    Threads::Threads
    PkgConfig::x264
    PkgConfig::libavcodec
    PkgConfig::libswscale
    PkgConfig::libavutil
    PkgConfig::libavformat
    oryx::oryx-crt-cpp
    mimalloc-static
    ${OpenCV_LIBS}
)

target_compile_options(common_runtime PUBLIC
    -Wall -Wextra -Wuninitialized -Wno-unused-function -Wunused-variable -Wno-interference-size -ftemplate-depth=2048 -fconstexpr-depth=2048 $<$<CONFIG:Release>:-g0> $<$<CONFIG:Release>:-O3>
)

target_link_options(common_runtime PUBLIC
    -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>
)

list(APPEND Exes server sender)

foreach(exe IN LISTS Exes)
    add_executable(rtmp_${exe}
        src/${exe}.cpp
    )   

    target_link_libraries(rtmp_${exe} PRIVATE
        common_runtime
    )
endforeach()