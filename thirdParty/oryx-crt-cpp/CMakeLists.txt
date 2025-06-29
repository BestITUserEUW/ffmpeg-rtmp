cmake_minimum_required(VERSION 3.29)

project(external-oryx-crt-cpp NONE)
include(ExternalProject)

list(APPEND CMAKE_ARGS
    "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DORYX_CRT_BUILD_SHARED_LIBS=OFF"
)

if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL "")
    list(APPEND CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
endif()

message(STATUS "Preparing project ${PROJECT_NAME} with args:")

foreach(arg ${CMAKE_ARGS})
    message(STATUS "-- ${arg}")
endforeach()

ExternalProject_add(
    ${PROJECT_NAME}
    GIT_REPOSITORY https://github.com/BestITUserEUW/oryx-crt-cpp.git
    GIT_TAG main
    CMAKE_ARGS "${CMAKE_ARGS}"
    GIT_SHALLOW ON
)