idf_component_register(
    SRCS 
        "GNSS.cpp"
        "DFRobot_GNSS.cpp"
        "GnssHardwareAdapter.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver freertos common sequent_i2c
)