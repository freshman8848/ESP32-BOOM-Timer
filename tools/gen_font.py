# -*- coding: utf-8 -*-
"""
生成 SPI-LCD 使用的点阵字库：
  - ASCII 8x16  (95 个字符, 0x20-0x7E)  -> font_ascii_8x16.c / .h
  - 中文 16x16 (本项目界面用到的汉字)     -> font_hz_16x16.c / .h
用法:  python tools/gen_font.py
输出:  components/lcd_font/ 目录下
"""
import os
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = r"C:\Windows\Fonts\simsun.ttc"
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "components", "lcd_font")
OUT_DIR = os.path.normpath(OUT_DIR)

# 界面用到的汉字集合（按使用顺序）
HZ_LIST = "倒计时秒表设置暂停运行结束分清零开始模式时间状态已空闲止中炸弹已拆除"

C_HEADER = """/*
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
"""


def load_font(size):
    return ImageFont.truetype(FONT_PATH, size)


def render_ascii(font, ch):
    """渲染单个 ASCII 字符到 8x16 点阵(行内1字节)。"""
    img = Image.new("L", (8, 16), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), ch, font=font, fill=255)
    px = img.load()
    out = []
    for y in range(16):
        b = 0
        for x in range(8):
            if px[x, y] >= 128:
                b |= (1 << (7 - x))
        out.append(b)
    return out


def render_hz(font, ch):
    """渲染单个汉字到 16x16 点阵(每行2字节), 自动居中。"""
    img = Image.new("L", (16, 16), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x0 = (16 - w) // 2 - bbox[0]
    y0 = (16 - h) // 2 - bbox[1]
    d.text((x0, y0), ch, font=font, fill=255)
    px = img.load()
    out = []
    for y in range(16):
        b0 = b1 = 0
        for x in range(16):
            if px[x, y] >= 128:
                if x < 8:
                    b0 |= (1 << (7 - x))
                else:
                    b1 |= (1 << (15 - x))
        out.append(b0)
        out.append(b1)
    return out


def write_c_array(f, name, rows, per_line_bytes):
    f.write("const uint8_t %s[%d][%d] = {\n" % (name, len(rows), per_line_bytes))
    for i, row in enumerate(rows):
        f.write("    {")
        for j, b in enumerate(row):
            f.write("0x%02X" % b)
            if j < len(row) - 1:
                f.write(", ")
        f.write("}%s\n" % ("," if i < len(rows) - 1 else ""))
    f.write("};\n\n")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    font16 = load_font(16)

    # ---------- ASCII 8x16 ----------
    ascii_rows = [render_ascii(font16, chr(c)) for c in range(0x20, 0x7F)]
    assert len(ascii_rows) == 95
    with open(os.path.join(OUT_DIR, "font_ascii_8x16.c"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("/* 自动生成, 请勿手工修改 */\n#include \"lcd_font_data.h\"\n\n")
        write_c_array(f, "font_ascii_8x16", ascii_rows, 16)

    # ---------- 中文 16x16 ----------
    hz_rows = [render_hz(font16, ch) for ch in HZ_LIST]
    with open(os.path.join(OUT_DIR, "font_hz_16x16.c"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("/* 自动生成, 请勿手工修改 */\n#include \"lcd_font_data.h\"\n\n")
        f.write("static const uint8_t hz_bitmap[%d][32] = {\n" % len(hz_rows))
        for i, row in enumerate(hz_rows):
            f.write("    {")
            for j, b in enumerate(row):
                f.write("0x%02X" % b)
                if j < len(row) - 1:
                    f.write(", ")
            f.write("}%s\n" % ("," if i < len(hz_rows) - 1 else ""))
        f.write("};\n\n")
        f.write("const hz16_entry_t hz_font_16x16[] = {\n")
        for i, ch in enumerate(HZ_LIST):
            f.write("    { 0x%04X /* %s */, hz_bitmap[%d] },\n" % (ord(ch), ch, i))
        f.write("};\n\n")
        f.write("const int hz_font_count = %d;\n" % len(hz_rows))

    with open(os.path.join(OUT_DIR, "lcd_font_data.h"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write(C_HEADER)

    print("OK  ->", OUT_DIR)
    print("ASCII chars : %d" % len(ascii_rows))
    print("Chinese chars: %d : %s" % (len(hz_rows), HZ_LIST))


if __name__ == "__main__":
    main()
