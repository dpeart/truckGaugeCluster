cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s3")

# Tell ESP-IDF where to find the shared components folder relative to this project
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smallGauges)

