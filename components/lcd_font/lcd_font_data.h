/*
 * 点阵字库 (由 tools/gen_font.py 自动生成, 请勿手工修改)
 * ASCII 8x16: 0x20 ~ 0x7E, 共 95 字符, 每个字符 16 字节(每行1字节, 高位在左)
 * 中文 16x16: 每个汉字 32 字节(每行2字节, 高位在左), 1 表示点亮
 */
#ifndef LCD_FONT_DATA_H
#define LCD_FONT_DATA_H

#include <stdint.h>
#include <stddef.h>

#define FONT_ASCII_W      8
#define FONT_ASCII_H      16
#define FONT_ASCII_NUM    95
#define FONT_HZ_W         16
#define FONT_HZ_H         16

extern const uint8_t font_ascii_8x16[FONT_ASCII_NUM][FONT_ASCII_H];

typedef struct {
    uint16_t code;      /* Unicode 编码 */
    const uint8_t *data; /* 32 字节点阵 */
} hz16_entry_t;

extern const hz16_entry_t hz_font_16x16[];
extern const int hz_font_count;

#endif /* LCD_FONT_DATA_H */
