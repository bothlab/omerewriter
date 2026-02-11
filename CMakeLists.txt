cmake_minimum_required(VERSION 3.19)
project(OMERewriter LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 23)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGLWidgets)

# extra requirements to silence warnings from the OME modules
find_package(Qt6 REQUIRED COMPONENTS Xml Core5Compat)

qt_standard_project_setup()

include(GNUInstallDirs)

include(FetchContent)
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


set(omemodel_BUILD_TESTING=OFF OFF CACHE BOOL "" FORCE)
set(xmldom "qt6" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(omecommon)
set(OMECommon_DIR "${omecommon_BINARY_DIR}" CACHE PATH "" FORCE)
list(PREPEND CMAKE_PREFIX_PATH "${omecommon_BINARY_DIR}")

FetchContent_MakeAvailable(omemodel)
set(OMEXML_DIR "${omemodel_BINARY_DIR}" CACHE PATH "" FORCE)
list(PREPEND CMAKE_PREFIX_PATH "${omemodel_BINARY_DIR}")

FetchContent_MakeAvailable(omefiles)

#
# Subdirs
#
add_subdirectory(src/)
