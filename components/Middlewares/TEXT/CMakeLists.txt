idf_component_register(
    SRCS fonts.c text.c
    INCLUDE_DIRS "."
    REQUIRES esp_partition spi_flash fatfs devices
)