cmake_minimum_required(VERSION 3.19)
project(OMERewriter
        LANGUAGES CXX
        DESCRIPTION "View and rewrite OME-TIFF metadata"
        VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Svg OpenGLWidgets)

# extra requirements to silence warnings from the OME modules
find_package(Qt6 REQUIRED COMPONENTS Xml Core5Compat)

qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)
include(FetchContent)

# Try to find ome-files-cpp in system locations first,
# and only fetch them if not found.
find_package(OMEFiles QUIET)

if(OMEFiles_FOUND)
    message(STATUS "Using environment OMEFiles")
else()
    message(STATUS "OME Files not found on system, fetching from source")

    FetchContent_Declare(
            omemodel
            GIT_REPOSITORY https://gitlab.com/codelibre/ome/ome-model.git
            GIT_TAG        99ab3fd25c27b3b625047bb0bc535201a611cd01
    )
    FetchContent_Declare(
            omecommon
            GIT_REPOSITORY https://gitlab.com/codelibre/ome/ome-common-cpp.git
            GIT_TAG        fa4cf3dacc72bd14037f2a48d42fd31bf182da2e
    )
    FetchContent_Declare(
            omefiles
            GIT_REPOSITORY https://gitlab.com/codelibre/ome/ome-files-cpp.git
            GIT_TAG        d1abffaa9c1d2143735ce254754410e591882bf2
    )

    # prefer Qt 6 for the XML parser
    set(xmldom "qt6" CACHE STRING "Use Qt6 for XML" FORCE)

    # We forcefully disable tests because omecommon offers no other way,
    # and the tests fail to build on Windows
    FetchContent_MakeAvailable(omecommon)
    file(READ "${omecommon_SOURCE_DIR}/CMakeLists.txt" omecommon_cmake)
    string(REGEX REPLACE "\n[ \t]*add_subdirectory\\(test\\)" "\n# add_subdirectory(test) # Disabled by parent" omecommon_cmake "${omecommon_cmake}")
    file(WRITE "${omecommon_SOURCE_DIR}/CMakeLists.txt" "${omecommon_cmake}")
    set(OMECommon_DIR "${omecommon_BINARY_DIR}" CACHE PATH "" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "${omecommon_BINARY_DIR}")

    FetchContent_MakeAvailable(omemodel)
    set(OMEXML_DIR "${omemodel_BINARY_DIR}" CACHE PATH "" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "${omemodel_BINARY_DIR}")

    # We disable documentation/tests/examples because building them also causes problems on Windows.
    # Again, the only way that works is an ugly hotpatch of the CMakeLists.txt file,
    # until a fix is merged upstream.
    FetchContent_MakeAvailable(omefiles)
    file(READ "${omefiles_SOURCE_DIR}/CMakeLists.txt" omefiles_cmake)
    string(REGEX REPLACE "\n[ \t]*add_subdirectory\\(docs/doxygen\\)" "\n# add_subdirectory(docs/doxygen) # Disabled by parent" omefiles_cmake "${omefiles_cmake}")
    string(REGEX REPLACE "\n[ \t]*add_subdirectory\\(docs/sphinx\\)" "\n# add_subdirectory(docs/sphinx) # Disabled by parent" omefiles_cmake "${omefiles_cmake}")
    string(REGEX REPLACE "\n[ \t]*add_subdirectory\\(examples\\)" "\n# add_subdirectory(examples) # Disabled by parent" omefiles_cmake "${omefiles_cmake}")
    string(REGEX REPLACE "\n[ \t]*add_subdirectory\\(test\\)" "\n# add_subdirectory(test) # Disabled by parent" omefiles_cmake "${omefiles_cmake}")
    file(WRITE "${omefiles_SOURCE_DIR}/CMakeLists.txt" "${omefiles_cmake}")

    # Patch CoreMetadata.h to include <memory>, fixes the Windows build
    file(READ "${omefiles_SOURCE_DIR}/lib/ome/files/CoreMetadata.h" coremetadata_h)
    if(NOT coremetadata_h MATCHES "#include <memory>")
        string(REGEX REPLACE
                "(#include <ome/xml/model/enums/PixelType.h>)"
                "\\1\n#include <memory>"
                coremetadata_h "${coremetadata_h}")
        file(WRITE "${omefiles_SOURCE_DIR}/lib/ome/files/CoreMetadata.h" "${coremetadata_h}")
    endif()
endif()

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
