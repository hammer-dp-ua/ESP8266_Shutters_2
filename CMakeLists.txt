# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

message("~~~ PATH: $ENV{PATH}")
message("~~~ ESPPORT: $ENV{ESPPORT}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(Shutters)