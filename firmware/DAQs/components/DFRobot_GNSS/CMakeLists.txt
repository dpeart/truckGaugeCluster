idf_component_register(
    SRCS 
        "DFRobot_GNSS.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_i2c_helpers
)