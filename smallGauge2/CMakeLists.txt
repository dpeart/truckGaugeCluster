cmake_minimum_required(VERSION 3.16)

# Make LV_CONF_INCLUDE_SIMPLE visible to ALL components, including LVGL
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smallGauge1)