cmake_minimum_required(VERSION 3.29)

project(ThirdParty)

include(cmake/build_target.cmake)

list(APPEND CMAKE_PREFIX_PATH "${DEPS_INSTALL_PATH}")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${DEPS_INSTALL_PATH}/lib/pkgconfig")

BuildTarget(ffnvcodec)
BuildTarget(x264)
BuildTarget(ffmpeg)
BuildTarget(opencv)
BuildTarget(mimalloc)
BuildTarget(oryx-crt-cpp)