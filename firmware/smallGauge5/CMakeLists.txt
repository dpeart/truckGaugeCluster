cmake_minimum_required(VERSION 3.16)

# Add this near the top of your root CMakeLists.txt
set_source_files_properties(
    "${IDF_PATH}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c" 
    PROPERTIES COMPILE_FLAGS "-O0"
)

# Make LV_CONF_INCLUDE_SIMPLE visible to ALL components, including LVGL
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smallGauge5)