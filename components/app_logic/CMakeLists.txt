idf_component_register(
    SRCS "app_logic.c" "app_ui.c"
    INCLUDE_DIRS "."
    REQUIRES common key_driver lcd_driver lcd_font buzzer_driver led_driver uart_cmd audio_player esp_timer
    EMBED_FILES "bg_image.bin"
)
