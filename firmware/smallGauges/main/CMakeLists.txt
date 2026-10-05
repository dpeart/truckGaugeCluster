# Standard shared sources
set(srcs
    "main.cpp"
    "ota_handler.c"

    # Shared application logic
    # "src/GaugePacket.c"
    "src/espnow_receiver.cpp"
    "src/lvgl_lock.c"
    "src/ui_actions.c"
    "src/wifi_ota.c"

    # Shared drivers
    "EXIO/TCA9554PWR.c"
    "Audio_Driver/PCM5101.c"
    "LCD_Driver/esp_lcd_st77916/esp_lcd_st77916.c"
    "LCD_Driver/ST77916.c"
    "Touch_Driver/esp_lcd_touch/esp_lcd_touch.c"
    "Touch_Driver/CST816.c"
    "LVGL_Driver/LVGL_Driver.c"
    "LVGL_UI/LVGL_Example.c"
    # "LVGL_UI/LVGL_Music.c"
    # "SD_Card/SD_MMC.c"
    "I2C_Driver/I2C_Driver.c"
    "PCF85063/PCF85063.c"
    "QMI8658/QMI8658.c"
    "BAT_Driver/BAT_Driver.c"
    "PWR_Key/PWR_Key.c"
    "Wireless/Wireless.c"
)

# Base include directories
set(include_dirs
    "Audio_Driver"
    # "MIC_Driver"
    "LCD_Driver/esp_lcd_st77916"
    "LCD_Driver"
    "Touch_Driver/esp_lcd_touch"
    "Touch_Driver"
    "LVGL_Driver"
    "LVGL_UI"
    "EXIO"
    "SD_Card"
    "I2C_Driver"
    "PCF85063"
    "QMI8658"
    "BAT_Driver"
    "PWR_Key"
    "Wireless"
    "src"
    "."
)

# -----------------------------------------------------------------------------
# Direct Variant Selection (CMake-only, no Kconfig)
# -----------------------------------------------------------------------------

# Default to variant 1 if no flag is passed in command line
set(GAUGE_VARIANT "1" CACHE STRING "Gauge variant")

# Set project name dynamically (used for SSID, hostname, and binary naming)
set(APP_PROJECT_NAME "smallGauge${GAUGE_VARIANT}")

# -----------------------------------------------------------------------------
# Per-Variant Sources & Includes
# -----------------------------------------------------------------------------
if(GAUGE_VARIANT EQUAL 1)
    set(GAUGE_FOLDER "smallGauge1")

    list(APPEND srcs
        "src/smallGauge1/updateUI.c"
        "src/smallGauge1/small_gauge.cpp"
        "src/smallGauge1/ui/images.c"
        "src/smallGauge1/ui/screens.c"
        "src/smallGauge1/ui/styles.c"
        "src/smallGauge1/ui/ui.c"
        "src/smallGauge1/ui/ui_font_roboto_condensed_50.c"
        "src/smallGauge1/ui/ui_font_roboto_mono_24.c"
        "src/smallGauge1/ui/ui_font_roboto_mono_30.c"
        "src/smallGauge1/ui/ui_font_roboto_mono_35.c"
        "src/smallGauge1/ui/ui_font_roboto_mono_40.c"
        "src/smallGauge1/ui/ui_font_robot_mono_20.c"
        "src/smallGauge1/ui/ui_image_center.c"
        "src/smallGauge1/ui/ui_image_engine_temp.c"
        "src/smallGauge1/ui/ui_image_fuel_location.c"
        "src/smallGauge1/ui/ui_image_oil_pressure.c"
    )

elseif(GAUGE_VARIANT EQUAL 2)
    set(GAUGE_FOLDER "smallGauge2")

    list(APPEND srcs
        "src/smallGauge2/updateUI.c"
        "src/smallGauge2/small_gauge.cpp"
        "src/smallGauge2/ui/images.c"
        "src/smallGauge2/ui/screens.c"
        "src/smallGauge2/ui/styles.c"
        "src/smallGauge2/ui/ui.c"
        "src/smallGauge2/ui/ui_font_roboto_condensed_50.c"
        "src/smallGauge2/ui/ui_font_roboto_mono_24.c"
        "src/smallGauge2/ui/ui_font_roboto_mono_30.c"
        "src/smallGauge2/ui/ui_font_roboto_mono_35.c"
        "src/smallGauge2/ui/ui_font_roboto_mono_40.c"
        "src/smallGauge2/ui/ui_font_robot_mono_20.c"
        "src/smallGauge2/ui/ui_image_battery_voltage.c"
        "src/smallGauge2/ui/ui_image_egt.c"
        "src/smallGauge2/ui/ui_image_iat.c"
    )

elseif(GAUGE_VARIANT EQUAL 3)
    set(GAUGE_FOLDER "smallGauge3")

    list(APPEND srcs
        "src/smallGauge3/updateUI.c"
        "src/smallGauge3/small_gauge.cpp"
        "src/smallGauge3/ui/images.c"
        "src/smallGauge3/ui/screens.c"
        "src/smallGauge3/ui/styles.c"
        "src/smallGauge3/ui/ui.c"
        "src/smallGauge3/ui/ui_font_roboto_condensed_50.c"
        "src/smallGauge3/ui/ui_font_roboto_mono_24.c"
        "src/smallGauge3/ui/ui_font_roboto_mono_30.c"
        "src/smallGauge3/ui/ui_font_roboto_mono_35.c"
        "src/smallGauge3/ui/ui_font_roboto_mono_40.c"
        "src/smallGauge3/ui/ui_font_robot_mono_20.c"
        "src/smallGauge3/ui/ui_image_fuel_pressure.c"
        "src/smallGauge3/ui/ui_image_trans_temp.c"
    )

elseif(GAUGE_VARIANT EQUAL 4)
    set(GAUGE_FOLDER "smallGauge4")

    list(APPEND srcs
        "src/smallGauge4/updateUI.c"
        "src/smallGauge4/small_gauge.cpp"
        "src/smallGauge4/ui/images.c"
        "src/smallGauge4/ui/screens.c"
        "src/smallGauge4/ui/styles.c"
        "src/smallGauge4/ui/ui.c"
        "src/smallGauge4/ui/ui_font_roboto_condensed_50.c"
        "src/smallGauge4/ui/ui_font_roboto_mono_30.c"
        "src/smallGauge4/ui/ui_image_fuel_pressure.c"
        "src/smallGauge4/ui/ui_image_oil_pressure.c"
        "src/smallGauge4/ui/ui_image_trans_temp.c"
        "src/smallGauge4/ui/ui_image_turbo.c"
    )

elseif(GAUGE_VARIANT EQUAL 5)
    set(GAUGE_FOLDER "smallGauge5")

    list(APPEND srcs
        "src/smallGauge5/updateUI.c"
        "src/smallGauge5/small_gauge.cpp"
        "src/smallGauge5/ui/images.c"
        "src/smallGauge5/ui/screens.c"
        "src/smallGauge5/ui/styles.c"
        "src/smallGauge5/ui/ui.c"
        "src/smallGauge5/ui/ui_font_roboto_condensed_50.c"
        "src/smallGauge5/ui/ui_font_roboto_mono_24.c"
        "src/smallGauge5/ui/ui_font_roboto_mono_30.c"
        "src/smallGauge5/ui/ui_font_roboto_mono_35.c"
        "src/smallGauge5/ui/ui_font_roboto_mono_40.c"
        "src/smallGauge5/ui/ui_font_robot_mono_20.c"
        "src/smallGauge5/ui/ui_image_battery_warning_light.c"
        "src/smallGauge5/ui/ui_image_engine_temp_icon.c"
        "src/smallGauge5/ui/ui_image_fuel_location_right.c"
        "src/smallGauge5/ui/ui_image_low_washer_fluid.c"
        "src/smallGauge5/ui/ui_image_od_off_indicator.c"
        "src/smallGauge5/ui/ui_image_oil_pressure_indicator.c"
        "src/smallGauge5/ui/ui_image_water_in_fuel.c"
    )

else()
    message(FATAL_ERROR "Invalid GAUGE_VARIANT '${GAUGE_VARIANT}' specified! Must be 1, 2, 3, 4, or 5.")
endif()

# Append UI search paths for active variant
list(APPEND include_dirs
    "src/${GAUGE_FOLDER}"
    "src/${GAUGE_FOLDER}/ui"
)

# Register component target
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES
        vehicle_protocol mdns esp_http_server wifi_provisioning app_update nvs_flash esp_wifi esp_event esp_netif driver esp_adc fatfs spi_flash
)

# Export APP_PROJECT_NAME as a C macro so wifi.c can use it
target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_PROJECT_NAME="${APP_PROJECT_NAME}")

# Attach compile definitions to the registered component target AFTER registration
add_custom_target(copy_gauge_bin ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
        "${CMAKE_BINARY_DIR}/${APP_PROJECT_NAME}.bin"
    COMMENT "Copying binary to: ${APP_PROJECT_NAME}.bin"
)

# 4. Force execution AFTER the app binary is completely generated by ESP-IDF
add_dependencies(copy_gauge_bin app)