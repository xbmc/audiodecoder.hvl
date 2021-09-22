cmake_minimum_required(VERSION 3.5)
project(audiodecoder.hvl)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

include(ExternalProject)

include_directories(${PROJECT_SOURCE_DIR}/lib
                    ${KODI_INCLUDE_DIR}/..)

add_subdirectory(lib/libhvl)

set(HVL_SOURCES src/HVLCodec.cpp)
set(HVL_HEADERS src/HVLCodec.h)

set(DEPLIBS hvl)

build_addon(audiodecoder.hvl HVL DEPLIBS)

include(CPack)
