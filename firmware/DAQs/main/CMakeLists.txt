idf_component_register(
    SRCS
    "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_event
        esp_netif
        esp_timer
        esp_wifi
        DFRobot_GNSS
        mcp960x
        SM_16DIGIN
        SM_16UNIVIN
        SM_RTD
        nvs_flash
)
