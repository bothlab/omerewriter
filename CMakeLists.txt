cmake_minimum_required(VERSION 3.19)
project(OMERewriter
        LANGUAGES CXX
        DESCRIPTION "View and rewrite OME-TIFF metadata"
        VERSION 0.1.1
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg OpenGLWidgets Xml)

# libtiff is used directly for reading and writing the TIFF container.
find_package(TIFF REQUIRED)

qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

#
# Version detection
#
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_tag
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_git_tag MATCHES "^v?([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
        set(PROJECT_VERSION_BASE "${CMAKE_MATCH_1}")
    else()
        set(PROJECT_VERSION_BASE "${PROJECT_VERSION}")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count "${_git_tag}..HEAD"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_commit_no
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_commit_hash
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_git_commit_no GREATER 0 AND _git_commit_hash)
        set(PROJECT_VERSION "${PROJECT_VERSION_BASE}+git${_git_commit_no}.${_git_commit_hash}")
    else()
        set(PROJECT_VERSION "${PROJECT_VERSION_BASE}")
    endif()
endif()

message(STATUS "OMERewriter version: ${PROJECT_VERSION}")

configure_file(
    "${CMAKE_SOURCE_DIR}/src/config.h.in"
    "${CMAKE_BINARY_DIR}/src/config.h"
    @ONLY
)

#
# Subdirs
#
add_subdirectory(src/)
add_subdirectory(data/)
