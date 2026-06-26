idf_component_register(
    SRCS
        "main.c"
        "c6_uart.c"
        "c6_modes.c"
        "ota_handler.c"
        "wifi_ota.c"
        "espnow_receiver.c"
        "telemetry_task.c"

    INCLUDE_DIRS
        "."
        # "Wireless"
        # "I2C_Driver"
        # "PCF85063"
        # "QMI8658"
        # "BAT_Driver"
        # "PWR_Key"

    REQUIRES
        driver
        esp_event
        esp_http_server
        app_update
        esp_netif
        esp_timer
        esp_wifi
        mdns
        nanopb
        nvs_flash
        wifi_provisioning
)
