cmake_minimum_required(VERSION 3.29)

project(external-ffmpeg NONE)
include(ExternalProject)
include(ProcessorCount)
ProcessorCount(NUM_PROC)

find_program(MAKE_EXE NAMES make)

set(FFMPEG_VERSION 7.1.1)
message(STATUS "FFMPEG_VERSION set to ${FFMPEG_VERSION}")

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(FFMPEG_PLATFORM "aarch64")
else()
    set(FFMPEG_PLATFORM "x86_64")
endif()

message(STATUS "FFMPEG_PLATFORM set to ${FFMPEG_PLATFORM}")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(FFMPEG_BUILD_TYPE "--debug")
else()
    set(FFMPEG_BUILD_TYPE "--release")
endif()

set(CONFIGURE_ARGS
    --arch=${FFMPEG_PLATFORM}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --extra-ldflags="-L${CMAKE_INSTALL_PREFIX}/lib"
    --extra-cflags="-I${CMAKE_INSTALL_PREFIX}/include"
    --extra-libs="-lpthread"
    --enable-nonfree
    --enable-gpl
    --disable-programs
    --disable-doc
    --disable-asm
    --enable-libx264
)

if(CMAKE_CROSSCOMPILING)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
        list(APPEND CONFIGURE_ARGS
            --target-os=linux
            --cc=aarch64-unknown-linux-gnu-gcc
            --cross-prefix=aarch64-unknown-linux-gnu-
            --enable-cross-compile
        )
    endif()
endif()

message(STATUS "Preparing project ${PROJECT_NAME} with args:")

foreach(CONFIGURE_ARG ${CONFIGURE_ARGS})
    message(STATUS "-- ${CONFIGURE_ARG}")
endforeach()

ExternalProject_add(
    ${PROJECT_NAME}
    URL https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CROSS_COMPILE="" <SOURCE_DIR>/configure ${CONFIGURE_ARGS}
    BUILD_COMMAND ${MAKE_EXE} -j${NUM_PROC}
    INSTALL_COMMAND ${MAKE_EXE} install -j${NUM_PROC}
)