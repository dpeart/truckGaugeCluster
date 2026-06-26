idf_component_register(
    SRCS
        "main.c"
        "ota_handler.c"

        # Application logic
        "src/updateUI.c"
        "src/GaugePacket.c"
        "src/espnow_receiver.c"
        "src/lvgl_lock.c"
        "src/ui_actions.c"
        "src/wifi_ota.c"

        # UI sources
        "src/ui/ui.c"
        "src/ui/screens.c"
        "src/ui/images.c"
        "src/ui/styles.c"

        # UI Images
        "src/ui/ui_image_battery_warning_light.c"
        "src/ui/ui_image_engine_temp_icon.c"
        "src/ui/ui_image_fuel_location_right.c"
        "src/ui/ui_image_low_washer_fluid.c"
        "src/ui/ui_image_od_off_indicator.c"
        "src/ui/ui_image_oil_pressure_indicator.c"
        "src/ui/ui_image_water_in_fuel.c"

        # Fonts
        "src/ui/ui_font_roboto_condensed_50.c"
        "src/ui/ui_font_roboto_condensed_25.c"
        "src/ui/ui_font_robot_mono_20.c"
        "src/ui/ui_font_roboto_mono_24.c"
        "src/ui/ui_font_roboto_mono_30.c"
        "src/ui/ui_font_roboto_mono_35.c"
        "src/ui/ui_font_roboto_mono_40.c"

        # Explicit driver sources (these MUST be listed)
        "./EXIO/TCA9554PWR.c"
        "./Audio_Driver/PCM5101.c"
        # "./MIC_Driver/MIC_Speech.c"

        "./LCD_Driver/esp_lcd_st77916/esp_lcd_st77916.c"
        "./LCD_Driver/ST77916.c"

        "./Touch_Driver/esp_lcd_touch/esp_lcd_touch.c"
        "./Touch_Driver/CST816.c"

        "./LVGL_Driver/LVGL_Driver.c"   # <-- LVGL_Init() lives here

        "./LVGL_UI/LVGL_Example.c"
        "./LVGL_UI/LVGL_Music.c"

        "./SD_Card/SD_MMC.c"

        "./I2C_Driver/I2C_Driver.c"
        "./PCF85063/PCF85063.c"
        "./QMI8658/QMI8658.c"

        "./BAT_Driver/BAT_Driver.c"
        "./PWR_Key/PWR_Key.c"

        "./Wireless/Wireless.c"

    INCLUDE_DIRS
        "./Audio_Driver"
        "./MIC_Driver"
        "./LCD_Driver/esp_lcd_st77916"
        "./LCD_Driver"
        "./Touch_Driver/esp_lcd_touch"
        "./Touch_Driver"
        "./LVGL_Driver"
        "./LVGL_UI"
        "./EXIO"
        "./SD_Card"
        "./I2C_Driver"
        "./PCF85063"
        "./QMI8658"
        "./BAT_Driver"
        "./PWR_Key"
        "./Wireless"
        "src"
        "src/ui"
        "."
    REQUIRES
        mdns esp_http_server wifi_provisioning app_update nvs_flash esp_wifi esp_event esp_netif driver esp_adc fatfs spi_flash
)
