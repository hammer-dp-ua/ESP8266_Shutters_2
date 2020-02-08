# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(ENV{PATH} "$ENV{GIT_PATH};$ENV{MINGW_BIN_PATH};$ENV{XTENSA_BIN_PATH};$ENV{IDF_PATH}\tools")
message(STATUS "~~~ PATH: $ENV{PATH}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(Shutters)