idf_component_register(
    SRCS "mcp960x.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_i2c_helpers)
