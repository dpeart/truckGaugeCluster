file(GLOB_RECURSE MAIN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

idf_component_register(
    SRCS ${MAIN_SOURCES}
    INCLUDE_DIRS "."
)
