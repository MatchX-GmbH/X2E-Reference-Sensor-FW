# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Include the body of stuff in the directory
# above, main and the tests component from below
set(EXTRA_COMPONENT_DIRS "components/ubxlib/port/platform/esp-idf/mcu/esp32/components" "components" "main")
#
#set(SDKCONFIG "sdkconfig" CACHE STRING "IDF sdkconfig file path")

# Reduce the component list to a minimum
#set(COMPONENTS "driver" "esptool_py" "unity" "ubxlib" "main" "heap" "lora_compon" "accel" "BQ27XXX")

# Set the components to include the "tests" for.
# This is intended to be overriden in the CMake cache
# by invoking idf.py with a line of the following form:
# idf.py -D TEST_COMPONENTS="ubxlib_runner" all
#set(TEST_COMPONENTS "replace-me"  CACHE STRING "Component to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(GPS_Tracker)
