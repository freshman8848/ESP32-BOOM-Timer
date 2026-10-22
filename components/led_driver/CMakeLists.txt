idf_component_register(
    SRCS "led_driver.c"
    INCLUDE_DIRS "."
    REQUIRES common esp_driver_gpio
)
