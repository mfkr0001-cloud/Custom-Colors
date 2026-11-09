cmake_minimum_required(VERSION 3.21)
project(CustomColors VERSION 1.1.0 LANGUAGES CXX)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Install the Geode SDK or use GitHub Actions.")
endif()

add_library(${PROJECT_NAME} SHARED src/main.cpp)
include($ENV{GEODE_SDK}/cmake/Geode.cmake)
setup_geode_mod(${PROJECT_NAME})
