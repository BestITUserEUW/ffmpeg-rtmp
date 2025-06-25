include(ProcessorCount)

function(BuildTarget TARGET_NAME)
    message(STATUS "Building Target: ${TARGET_NAME}")
    set(TARGET_SOURCE_DIR "${PROJECT_SOURCE_DIR}/${TARGET_NAME}")
    set(TARGET_BINARY_DIR "${DEPS_BUILD_PATH}/${TARGET_NAME}")
    set(TARGET_INSTALL_DIR "${DEPS_INSTALL_PATH}")

    file(MAKE_DIRECTORY ${TARGET_BINARY_DIR})

    list(APPEND CONFIGURE_ARGS
        "-G${CMAKE_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX:PATH=${TARGET_INSTALL_DIR}"
        "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}"
    )

    if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL "")
        list(APPEND CONFIGURE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()

    if(DEFINED CMAKE_GENERATOR_PLATFORM AND NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "")
        list(APPEND CONFIGURE_ARGS "-A${CMAKE_GENERATOR_PLATFORM}")
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND}
        ${CONFIGURE_ARGS}
        "${TARGET_SOURCE_DIR}"
        WORKING_DIRECTORY "${TARGET_BINARY_DIR}"
        RESULT_VARIABLE CONFIGURE_FAILED
    )

    if(${CONFIGURE_FAILED})
        message(FATAL_ERROR "${TARGET_SOURCE_DIR} failed to configure!")
    endif()

    ProcessorCount(NUM_PROCS)
    set(ENV{MAKEFLAGS} -j${NUM_PROCS})

    if(MSVC)
        execute_process(COMMAND ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
            WORKING_DIRECTORY ${TARGET_BINARY_DIR}
            RESULT_VARIABLE BUILD_FAILED
        )
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} --build .
            WORKING_DIRECTORY ${TARGET_BINARY_DIR}
            RESULT_VARIABLE BUILD_FAILED
        )
    endif()

    if(${BUILD_FAILED})
        message(FATAL_ERROR "${TARGET_BINARY_DIR} failed to build!")
    endif()
endfunction()