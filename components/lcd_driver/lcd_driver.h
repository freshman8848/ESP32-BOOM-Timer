#pragma once

/**
 * @file lcd_driver.h
 * @brief SPI-LCD 驱动与绘图接口 (ST7789V2, 320x240 横屏, RGB565)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

/* RGB565 颜色宏 */
#define LCD_COLOR(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

#define LCD_COLOR_BLACK   LCD_COLOR(0, 0, 0)
#define LCD_COLOR_WHITE   LCD_COLOR(255, 255, 255)
#define LCD_COLOR_RED     LCD_COLOR(255, 0, 0)
#define LCD_COLOR_GREEN   LCD_COLOR(0, 255, 0)
#define LCD_COLOR_BLUE    LCD_COLOR(0, 0, 255)
#define LCD_COLOR_CYAN    LCD_COLOR(0, 255, 255)
#define LCD_COLOR_YELLOW  LCD_COLOR(255, 255, 0)
#define LCD_COLOR_GRAY    LCD_COLOR(128, 128, 128)
#define LCD_COLOR_DGRAY   LCD_COLOR(60, 60, 60)

/** @brief 初始化 SPI 总线与 ST7789V2, 打开背光 (背光经 XL9555 P1_3) */
esp_err_t lcd_driver_init(void);

/** @brief 全屏填充 */
void lcd_fill_screen(uint16_t color);

/**
 * @brief 在 (x, y) 处绘制 (w x h) 的图像 (数据为大端 RGB565 字节流, 与 LCD 帧格式一致)
 * @param data 图像数据, 长度须 >= w*h*2
 */
void lcd_draw_rgb565(int x, int y, int w, int h, const uint8_t *data);

/** @brief 填充矩形 (含边界) */
void lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t color);

/** @brief 画单个像素 */
void lcd_draw_pixel(int x, int y, uint16_t color);

/** @brief 画水平/垂直线 */
void lcd_draw_hline(int x0, int x1, int y, uint16_t color);
void lcd_draw_vline(int x, int y0, int y1, uint16_t color);

/** @brief 画空心圆 (中点画圆) */
void lcd_draw_circle(int cx, int cy, int r, uint16_t color);

/** @brief 画线段 (Bresenham) */
void lcd_draw_line(int x0, int y0, int x1, int y1, uint16_t color);

/**
 * @brief 在 (x, y) 处绘制 8x16 ASCII 字符 (左上角为原点)
 */
void lcd_draw_char(int x, int y, char ch, uint16_t fg, uint16_t bg);

/**
 * @brief 在 (x, y) 处绘制 16x16 汉字
 * @param code Unicode 码点; 未收录的字返回 false 且不绘制
 */
bool lcd_draw_hz(int x, int y, uint16_t code, uint16_t fg, uint16_t bg);

/**
 * @brief 绘制字符串, 自动混排 ASCII 与 UTF-8 中文
 * @return 实际占用的像素宽度
 */
int lcd_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg);

/**
 * @brief 把 16x16 汉字放大 scale 倍绘制 (透明背景, 用于全屏大字)
 *        字宽/高为 16*scale; 未收录的字不绘制
 */
void lcd_draw_hz_scaled(int x, int y, uint16_t code, int scale,
                        uint16_t fg, uint16_t bg);

/**
 * @brief 把 8x16 ASCII 字符放大 scale 倍绘制 (用于大号倒计时数字)
 *        数字区域 8*scale 宽, 16*scale 高
 */
void lcd_draw_char_scaled(int x, int y, char ch, int scale,
                          uint16_t fg, uint16_t bg);

/** @brief 背光开关 (经 XL9555) */
void lcd_backlight(bool on);
