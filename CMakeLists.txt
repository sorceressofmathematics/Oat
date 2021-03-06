cmake_minimum_required (VERSION 2.8)

# Project data
project (Oat C CXX)

# Internal version list
set (VERSION_LIST "${GIT_BRANCH}.${GIT_COMMIT_HASH}")

# Generate Oat version number: Major.Branch.Commit
find_package (Git REQUIRED)

# Get the current working branch
execute_process (
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of working branch
execute_process (
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set compiler options
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -Wextra")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# Make sure a build type has been specified
if (NOT CMAKE_BUILD_TYPE )
   set (CMAKE_BUILD_TYPE Release ... FORCE )
endif()

# Build type case insenstivity
string(TOLOWER ${CMAKE_BUILD_TYPE} LOWERCASE_CMAKE_BUILD_TYPE)

# Swich compiler flags based on build type
if (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "debug")
    set (FULL_CXX_FLAG_LIST "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (LOWERCASE_CMAKE_BUILD_TYPE STREQUAL "release")
    set (FULL_CXX_FLAG_LIST "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
    set (BUILD_TESTS OFF)
else()
    message (FATAL_ERROR "Set and acceptable build type using -DCMAKE_BUILD_TYPE=<debug or release>")
endif()

# Configure a header file to pass CMake settings to source code containing
# build options and version info
configure_file (lib/utility/OatConfig.h.in "${PROJECT_BINARY_DIR}/OatConfig.h")
include_directories("${PROJECT_BINARY_DIR}") # Config.h ends up in BINARY_DIR

# Build options
option (USE_FLYCAP "Compile with support for Point-Grey cameras" OFF)
option (BUILD_TESTS "Build and run tests." ON)
option (BUILD_DOCS "Build doxygen documentation." OFF)

# Show options summary
message (STATUS "Oat version: ${VERSION_LIST}")
message (STATUS "Compilation options:" )
message (STATUS "  Build type: ${LOWERCASE_CMAKE_BUILD_TYPE}")
message (STATUS "  Compile with Point Grey Support: ${USE_FLYCAP}")
message (STATUS "  Build tests: ${BUILD_TESTS}")
message (STATUS "  Build documentation: ${BUILD_DOCS}")

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
if(CMAKE_THREAD_LIBS_INIT)
    set (Thread_LIBS ${CMAKE_THREAD_LIBS_INIT} rt)
    message (STATUS "Found threading libs: ${Thread_LIBS}")
endif()

# Boost, minimum version 1.56 for type_index.hpp
find_package (Boost 1.56.0 REQUIRED system thread program_options filesystem)
include_directories (${Boost_INCLUDE_DIRS})
link_directories (${Boost_LIBRARY_DIR})

# OpenCV
find_package (OpenCV REQUIRED)
if (OpenCV_FOUND)
    message (STATUS "Found OpenCV")
else ()
    message (FATAL_ERROR "OpenCV not found")
endif ()

if (OpenCV_CUDA_VERSION)
    message (STATUS "   OpenCV has CUDA support.")
    set (OAT_USE_CUDA ON)
else ()
    message (STATUS "   CUDA support not enabled.")
    set (OAT_USE_CUDA OFF)
endif ()

#find_library (ZMQ_LIB libzmq)
#if ()
#    message (STATUS "Found OpenCV")
#else (OpenCV_FOUND)
#    message (FATAL_ERROR "OpenCV not found")
#endif ()

# Flycapture
if (${USE_FLYCAP})
    # Required for point-grey cameras
    # TODO: Need to provide a user flag to specify flycapture dir
    include_directories ("/usr/include/flycapture")
    find_library(FLYCAPTURE2 flycapture)

    if (FLYCAPTURE2)
        message (STATUS "Found Flycapture.")
    else (FLYCAPTURE2)
        message (FATAL_ERROR "Flycapture not found. Did you install in /usr/include/flycapture?")
    endif ()
endif ()

# Include dirs
set (EXT_PROJECTS_DIR ${PROJECT_SOURCE_DIR}/ext)

# Include cpptoml
add_subdirectory (${EXT_PROJECTS_DIR}/cpptoml)
include_directories (${CPPTOML_INCLUDE_DIR})

# Include rapidjson
add_subdirectory (${EXT_PROJECTS_DIR}/rapidjson)
include_directories (${RAPIDJSON_INCLUDE_DIR})

# Include local libs
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/lib/shmemdf)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/lib/utility)

# Common libraries for all Oat components
set (OatCommon_LIBS ${OpenCV_LIBS} ${Boost_LIBRARIES} ${Thread_LIBS})

# Oat components
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/cleaner)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/decorator)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/framefilter)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/frameserver)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/frameviewer)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/positioncombiner)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/positiondetector)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/positionfilter)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/positiongenerator)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/recorder)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/positionsocket)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/calibrator)
add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/src/buffer)

# All executables should be installed in Oat/oat/libexec
set (CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/../oat/libexec" CACHE PATH "Default install path" FORCE)

# Testin
if (${BUILD_TESTS})
    # Catch (and enables unit testing)
    add_subdirectory(${EXT_PROJECTS_DIR}/catch)
    enable_testing(true)

    set(TESTING_INCLUDES ${CATCH_INCLUDE_DIR} )
    function(add_oat_test name libs)
        include_directories(${TESTING_INCLUDES})
        add_executable(${name}_test ${name}_test.cpp)
        target_link_libraries (${name}_test ${libs})
        add_test(${name}_test ${name}_test)
    endfunction()

    # Tests
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

endif()

# API documentation
if (${BUILD_DOCS})
    add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/doc")
endif()

# CPack Deb package
set (CPACK_GENERATOR "DEB")
set (CPACK_DEBIAN_PACKAGE_MAINTAINER "jonnew") #required
