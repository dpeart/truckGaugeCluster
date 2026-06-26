idf_component_register(
    SRCS
        "./main.cpp"
        "./slaveBootHandler.cpp"
        "./p4_uart.cpp"
        "./p4_modes.cpp"
        "./p4_telemetry.cpp"
        "./p4_ota.cpp"

    INCLUDE_DIRS
        "."

    REQUIRES
        app_update
        driver
        esp_timer
        nanopb
)
