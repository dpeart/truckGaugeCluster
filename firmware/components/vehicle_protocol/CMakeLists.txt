idf_component_register(SRCS "vcan_receiver.c" "vehicle_protocol.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_timer esp_wifi freertos)