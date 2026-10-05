idf_component_register(SRCS "vcan_receiver.cpp" "vcan_sender.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_timer esp_wifi freertos)