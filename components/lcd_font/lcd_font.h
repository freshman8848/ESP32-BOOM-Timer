#pragma once

/**
 * @file lcd_font.h
 * @brief 点阵字库访问接口 (数据由 tools/gen_font.py 生成)
 */
#include "lcd_font_data.h"

/**
 * @brief 按 Unicode 码点查找 16x16 汉字点阵
 * @param code Unicode 码点 (如 '倒' = 0x5012)
 * @return 汉字点阵条目指针; 未收录返回 NULL
 */
const hz16_entry_t *lcd_font_find_hz(uint16_t code);
