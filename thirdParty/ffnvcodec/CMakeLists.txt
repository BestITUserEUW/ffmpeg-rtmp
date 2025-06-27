cmake_minimum_required(VERSION 3.29)

project(external-ffnvcodec NONE)
include(ExternalProject)
include(ProcessorCount)
ProcessorCount(NUM_PROC)

find_program(MAKE_EXE NAMES make)

ExternalProject_add(
    ${PROJECT_NAME}
    GIT_REPOSITORY https://github.com/FFmpeg/nv-codec-headers.git
    GIT_TAG n13.0.19.0
    GIT_SHALLOW ON
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${MAKE_EXE} install PREFIX=${CMAKE_INSTALL_PREFIX}
    BUILD_IN_SOURCE 1
)