idf_component_register(
    SRCS "lcd_driver.c"
    INCLUDE_DIRS "."
    REQUIRES common lcd_font xl9555 esp_driver_spi esp_driver_gpio
)
