#include "lcd_font.h"

const hz16_entry_t *lcd_font_find_hz(uint16_t code)
{
    for (int i = 0; i < hz_font_count; i++) {
        if (hz_font_16x16[i].code == code) {
            return &hz_font_16x16[i];
        }
    }
    return NULL;
}
