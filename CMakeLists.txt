# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "main/components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(VL53L4CD_Project)
