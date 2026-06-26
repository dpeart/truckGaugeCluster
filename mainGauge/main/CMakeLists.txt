idf_component_register(
    SRCS
        "./main.cpp"
        "./src/slaveBootHandler.cpp"
        "./src/p4_uart.cpp"
        "./src/p4_modes.cpp"
        "./src/p4_telemetry.cpp"
        "./src/p4_ota.cpp"
        "./src/GaugePacket.cpp"
        "./src/updateSpeed.cpp"
        "./src/ui/images.c"
        "./src/ui/screens.c"
        "./src/ui/styles.c"
        "./src/ui/ui.c"
        "./src/ui_actions.cpp"
        "./src/ui/ui_font_roboto_condensed_50.c"
        "./src/ui/ui_font_roboto_mono_24.c"
        "./src/ui/ui_font_roboto_mono_30.c"
        "./src/ui/ui_font_roboto_mono_35.c"
        "./src/ui/ui_font_roboto_mono_40.c"
        "./src/ui/ui_image_center.c"
        "./src/ui/ui_image_center_glow_ring.c"
        "./src/ui/ui_image_left.c"
        "./src/ui/ui_image_right.c"
        "./src/ui/ui_image_high_beam.c"
        "./src/ui/ui_image_main_gauge_bg.c"

    INCLUDE_DIRS
        "."
        "./src"
        "./src/ui"

    REQUIRES
        app_update
        driver
        esp_timer
        nvs_flash
)

        # "./src/espnow_receiver.cpp"
        # "./src/espnow_task.cpp"
