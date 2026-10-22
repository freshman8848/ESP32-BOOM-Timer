# -*- coding: utf-8 -*-
"""验证生成的字库点阵数据完整性: 每个字形非全零, 数量正确"""
import re

def parse_rows(s, marker, end_marker):
    seg = s[s.index(marker):s.index(end_marker)]
    return re.findall(r'\{([^{}]*)\}', seg)

def bytes_of(row):
    return re.findall(r'0x[0-9A-F]{2}', row)

root = r'D:\ESP32\Project\END\Boom\BOOM\components\lcd_font'
hz = open(root + r'\font_hz_16x16.c', encoding='utf-8').read()
ascii_f = open(root + r'\font_ascii_8x16.c', encoding='utf-8').read()

hz_rows = parse_rows(hz, 'hz_bitmap', 'hz_font_16x16')
ascii_rows = parse_rows(ascii_f, 'font_ascii_8x16', '};\n')

print('hz glyphs:', len(hz_rows), '| bytes/glyph:', len(bytes_of(hz_rows[0])))
empty_hz = [i for i, r in enumerate(hz_rows) if all(b == '0x00' for b in bytes_of(r))]
print('hz empty glyphs:', empty_hz if empty_hz else 'none')

print('ascii glyphs:', len(ascii_rows), '| bytes/glyph:', len(bytes_of(ascii_rows[0])))
empty_ascii = sum(1 for r in ascii_rows if all(b == '0x00' for b in bytes_of(r)))
print('ascii empty glyphs:', empty_ascii)

# 抽查: '倒'(0x5012) 与 '表'(0x8868) 非空
codes = []
for m in re.finditer(r'\{ 0x([0-9A-F]{4})', hz):
    codes.append(int(m.group(1), 16))
print('unicode codes:', len(codes), '| first 8:', [hex(c) for c in codes[:8]])
