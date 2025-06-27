cmake_minimum_required(VERSION 3.29)

project(external-x264 NONE)
include(ExternalProject)
include(ProcessorCount)
ProcessorCount(NUM_PROC)

find_program(MAKE_EXE NAMES make)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(X264_PLATFORM "linux-aarch64")
else()
    set(X264_PLATFORM "linux-x86_64")
endif()

message(STATUS "X264_PLATFORM set to ${X264_PLATFORM}")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(X264_BUILD_TYPE "--debug")
else()
    set(X264_BUILD_TYPE "--release")
endif()

message(STATUS "X264_BUILD_TYPE set to ${X264_BUILD_TYPE}")

set(CONFIGURE_ARGS
    --prefix=${CMAKE_INSTALL_PREFIX}
    --exec-prefix=${CMAKE_INSTALL_PREFIX}
    --enable-static
    --enable-pic
    --disable-cli
    --disable-asm
)

if(CMAKE_CROSSCOMPILING)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        list(APPEND CONFIGURE_ARGS
            --host=aarch64-linux-gnu
            --cross-prefix=aarch64-unknown-linux-gnu-
        )
    endif()
endif()

message(STATUS "Preparing project ${PROJECT_NAME} with args:")

foreach(CONFIGURE_ARG ${CONFIGURE_ARGS})
    message(STATUS "-- ${CONFIGURE_ARG}")
endforeach()

ExternalProject_add(
    ${PROJECT_NAME}
    GIT_REPOSITORY https://code.videolan.org/videolan/x264.git
    GIT_TAG master
    GIT_SHALLOW ON
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CROSS_COMPILE="" <SOURCE_DIR>/configure ${CONFIGURE_ARGS}
    BUILD_COMMAND ${MAKE_EXE} -j${NUM_PROC}
    INSTALL_COMMAND ${MAKE_EXE} install -j${NUM_PROC}
)