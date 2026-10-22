idf_component_register(
    SRCS "audio_player.c" "es8388.c"
    INCLUDE_DIRS "include" "."
    REQUIRES common xl9555 esp_driver_i2s esp_driver_i2c
    EMBED_FILES "sounds/snd_boot.wav"
                "sounds/snd_start.wav"
                "sounds/snd_over.wav"
                "sounds/snd_success.wav"
                "sounds/snd_explosion.wav"
)
