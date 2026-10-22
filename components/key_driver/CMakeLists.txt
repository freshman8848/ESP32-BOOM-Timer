idf_component_register(
    SRCS "key_driver.c"
    INCLUDE_DIRS "."
    REQUIRES common xl9555 esp_driver_gpio esp_timer
)
