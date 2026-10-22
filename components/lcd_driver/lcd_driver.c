#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "lcd_driver.h"
#include "lcd_font.h"
#include "xl9555.h"
#include "pins.h"

static const char *TAG = "lcd";

#define SPI_LCD_HOST      SPI2_HOST
#define SPI_LCD_FREQ_HZ   40 * 1000 * 1000
#define PUSH_BUF_SIZE     512   /* 像素缓冲, 单位 uint16_t */

static spi_device_handle_t s_lcd_spi = NULL;
static uint16_t s_push_buf[PUSH_BUF_SIZE];

/* -------------------- 底层 SPI 写 -------------------- */

static void lcd_set_dc(bool data)
{
    gpio_set_level(PIN_LCD_DC, data ? 1 : 0);
}

static void lcd_write_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    lcd_set_dc(false);
    spi_device_transmit(s_lcd_spi, &t);
}

static void lcd_write_data(const uint8_t *data, size_t len)
{
    lcd_set_dc(true);
    while (len > 0) {
        size_t chunk = len > 64 ? 64 : len;
        spi_transaction_t t = {
            .length = chunk * 8,
            .tx_buffer = data,
        };
        spi_device_transmit(s_lcd_spi, &t);
        data += chunk;
        len -= chunk;
    }
}

static void lcd_write_data8(uint8_t d)
{
    lcd_write_data(&d, 1);
}

/* -------------------- 初始化 -------------------- */

static void lcd_init_sequence(void)
{
    /* 硬件复位: RST 经 XL9555 P1_2 (板上 SLCD_RST), 拉低 >= 10us 后拉高 */
    xl9555_pin_write(XL9555_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    xl9555_pin_write(XL9555_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_write_cmd(0x01);                          /* SWRESET */
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_write_cmd(0x11);                          /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_write_cmd(0x3A); lcd_write_data8(0x05);   /* COLMOD: 16bit RGB565 */

    /* MADCTL: 横屏 320x240, RGB 顺序 (MV=1)
       若显示颜色偏色/方向不对, 可改 0x68 (BGR) 或调整翻转位 */
    lcd_write_cmd(0x36); lcd_write_data8(0x60);

    lcd_write_cmd(0xB2);                          /* PORCTL */
    {
        static const uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        lcd_write_data(d, sizeof(d));
    }
    lcd_write_cmd(0xB7); lcd_write_data8(0x35);   /* GCTRL */
    lcd_write_cmd(0xBB); lcd_write_data8(0x19);   /* VCOMS */
    lcd_write_cmd(0xC0); lcd_write_data8(0x2C);   /* LCMCTRL */
    lcd_write_cmd(0xC2); lcd_write_data8(0x01);   /* VDVVRHEN */
    lcd_write_cmd(0xC3); lcd_write_data8(0x12);   /* VRHS */
    lcd_write_cmd(0xC4); lcd_write_data8(0x20);   /* VDVS */
    lcd_write_cmd(0xC6); lcd_write_data8(0x0F);   /* FRCTRL2 */
    lcd_write_cmd(0xD0);                          /* PWCTRL1 */
    {
        static const uint8_t d[] = {0xA4, 0xA1};
        lcd_write_data(d, sizeof(d));
    }
    lcd_write_cmd(0xE0);                          /* PVGAMCTRL */
    {
        static const uint8_t d[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
                                    0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
        lcd_write_data(d, sizeof(d));
    }
    lcd_write_cmd(0xE1);                          /* NVGAMCTRL */
    {
        static const uint8_t d[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
                                    0x44, 0x51, 0x1F, 0x1F, 0x1F, 0x1F, 0x23};
        lcd_write_data(d, sizeof(d));
    }

    lcd_write_cmd(0x21);                          /* INVON (ST7789 需反显) */
    lcd_write_cmd(0x13);                          /* NORON */
    lcd_write_cmd(0x29);                          /* DISPON */
}

esp_err_t lcd_driver_init(void)
{
    /* DC 脚输出; 复位脚经 XL9555 控制, 见 lcd_init_sequence() */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_LCD_DC),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    /* 背光/复位由 XL9555 控制, 初始化扩展芯片 */
    esp_err_t ret = xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }
    lcd_backlight(false);

    /* SPI 总线 */
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PUSH_BUF_SIZE * 2,
    };
    ret = spi_bus_initialize(SPI_LCD_HOST, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t dev = {
        .mode = 0,
        .clock_speed_hz = SPI_LCD_FREQ_HZ,
        .spics_io_num = PIN_LCD_CS,
        .queue_size = 4,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ret = spi_bus_add_device(SPI_LCD_HOST, &dev, &s_lcd_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    lcd_init_sequence();
    lcd_fill_screen(LCD_COLOR_BLACK);
    lcd_backlight(true);
    ESP_LOGI(TAG, "ST7789V2 init ok (320x240 landscape)");
    return ESP_OK;
}

/* -------------------- 窗口与像素推送 -------------------- */

static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t b[4];
    lcd_write_cmd(0x2A);                          /* CASET */
    b[0] = x0 >> 8; b[1] = x0 & 0xFF; b[2] = x1 >> 8; b[3] = x1 & 0xFF;
    lcd_write_data(b, 4);

    lcd_write_cmd(0x2B);                          /* PASET */
    b[0] = y0 >> 8; b[1] = y0 & 0xFF; b[2] = y1 >> 8; b[3] = y1 & 0xFF;
    lcd_write_data(b, 4);

    lcd_write_cmd(0x2C);                          /* RAMWR */
}

static void lcd_push_color(const uint16_t *colors, size_t n)
{
    static uint8_t bytes[PUSH_BUF_SIZE * 2];
    lcd_set_dc(true);
    size_t done = 0;
    while (done < n) {
        size_t chunk = n - done;
        if (chunk > PUSH_BUF_SIZE) {
            chunk = PUSH_BUF_SIZE;
        }
        for (size_t i = 0; i < chunk; i++) {
            bytes[i * 2]     = colors[done + i] >> 8;   /* RGB565 高字节在前 */
            bytes[i * 2 + 1] = colors[done + i] & 0xFF;
        }
        spi_transaction_t t = {
            .length = chunk * 16,
            .tx_buffer = bytes,
        };
        spi_device_transmit(s_lcd_spi, &t);
        done += chunk;
    }
}

/* -------------------- 绘图 API -------------------- */

void lcd_fill_screen(uint16_t color)
{
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (size_t i = 0; i < PUSH_BUF_SIZE; i++) {
        s_push_buf[i] = color;
    }
    size_t total = (size_t)LCD_WIDTH * LCD_HEIGHT;
    size_t done = 0;
    while (done < total) {
        size_t n = (total - done) > PUSH_BUF_SIZE ? PUSH_BUF_SIZE
                                                  : (total - done);
        lcd_push_color(s_push_buf, n);
        done += n;
    }
}

void lcd_draw_rgb565(int x, int y, int w, int h, const uint8_t *data)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    x = x < 0 ? 0 : x;
    y = y < 0 ? 0 : y;
    if (x + w > LCD_WIDTH)  { w = LCD_WIDTH - x; }
    if (y + h > LCD_HEIGHT) { h = LCD_HEIGHT - y; }
    if (w <= 0 || h <= 0) {
        return;
    }
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    size_t total = (size_t)w * h * 2;
    size_t done = 0;
    lcd_set_dc(true);
    while (done < total) {
        size_t n = (total - done) > PUSH_BUF_SIZE * 2 ? PUSH_BUF_SIZE * 2
                                                      : (total - done);
        spi_transaction_t t = {
            .length = n * 8,
            .tx_buffer = data + done,
        };
        spi_device_transmit(s_lcd_spi, &t);
        done += n;
    }
}

void lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 >= LCD_WIDTH  ? LCD_WIDTH  - 1 : x1;
    y1 = y1 >= LCD_HEIGHT ? LCD_HEIGHT - 1 : y1;
    if (x1 < x0 || y1 < y0) {
        return;
    }
    for (size_t i = 0; i < PUSH_BUF_SIZE; i++) {
        s_push_buf[i] = color;
    }
    size_t w = (size_t)(x1 - x0 + 1);
    size_t h = (size_t)(y1 - y0 + 1);
    lcd_set_window(x0, y0, x1, y1);
    size_t total = w * h;
    size_t done = 0;
    while (done < total) {
        size_t n = (total - done) > PUSH_BUF_SIZE ? PUSH_BUF_SIZE
                                                  : (total - done);
        lcd_push_color(s_push_buf, n);
        done += n;
    }
}

void lcd_draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
        return;
    }
    lcd_set_window(x, y, x, y);
    lcd_push_color(&color, 1);
}

void lcd_draw_hline(int x0, int x1, int y, uint16_t color)
{
    lcd_fill_rect(x0, y, x1, y, color);
}

void lcd_draw_vline(int x, int y0, int y1, uint16_t color)
{
    lcd_fill_rect(x, y0, x, y1, color);
}

void lcd_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        lcd_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void lcd_draw_circle(int cx, int cy, int r, uint16_t color)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        lcd_draw_pixel(cx + x, cy + y, color);
        lcd_draw_pixel(cx - x, cy + y, color);
        lcd_draw_pixel(cx + x, cy - y, color);
        lcd_draw_pixel(cx - x, cy - y, color);
        lcd_draw_pixel(cx + y, cy + x, color);
        lcd_draw_pixel(cx - y, cy + x, color);
        lcd_draw_pixel(cx + y, cy - x, color);
        lcd_draw_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void lcd_draw_char(int x, int y, char ch, uint16_t fg, uint16_t bg)
{
    if (ch < 0x20 || ch > 0x7E) {
        ch = ' ';
    }
    const uint8_t *bmp = font_ascii_8x16[ch - 0x20];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bmp[row] & (0x80 >> col)) {
                lcd_draw_pixel(x + col, y + row, fg);
            }
        }
    }
}

bool lcd_draw_hz(int x, int y, uint16_t code, uint16_t fg, uint16_t bg)
{
    const hz16_entry_t *e = lcd_font_find_hz(code);
    if (e == NULL) {
        return false;
    }
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            if (e->data[row * 2] & (0x80 >> col)) {
                lcd_draw_pixel(x + col, y + row, fg);
            } else if (e->data[row * 2 + 1] & (0x80 >> (col - 8))) {
                lcd_draw_pixel(x + col, y + row, fg);
            }
        }
    }
    return true;
}

void lcd_draw_hz_scaled(int x, int y, uint16_t code, int scale,
                        uint16_t fg, uint16_t bg)
{
    const hz16_entry_t *e = lcd_font_find_hz(code);
    if (e == NULL || scale < 1) {
        return;
    }
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            bool on = (col < 8)
                          ? (e->data[row * 2] & (0x80 >> col))
                          : (e->data[row * 2 + 1] & (0x80 >> (col - 8)));
            if (on) {
                lcd_fill_rect(x + col * scale, y + row * scale,
                              x + col * scale + scale - 1,
                              y + row * scale + scale - 1, fg);
            }
        }
    }
}

int lcd_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg)
{
    int cx = x;
    while (*str) {
        unsigned char c = (unsigned char)*str;
        if (c < 0x80) {
            lcd_draw_char(cx, y, (char)c, fg, bg);
            cx += 8;
            str++;
        } else if ((c & 0xE0) == 0xC0 && str[1]) {       /* 2 字节 */
            uint16_t code = ((c & 0x1F) << 6) | (str[1] & 0x3F);
            lcd_draw_hz(cx, y, code, fg, bg);
            cx += 16;
            str += 2;
        } else if ((c & 0xF0) == 0xE0 && str[1] && str[2]) { /* 3 字节 */
            uint16_t code = ((c & 0x0F) << 12) |
                            ((str[1] & 0x3F) << 6) |
                            (str[2] & 0x3F);
            lcd_draw_hz(cx, y, code, fg, bg);
            cx += 16;
            str += 3;
        } else {
            str++;
        }
    }
    return cx - x;
}

void lcd_draw_char_scaled(int x, int y, char ch, int scale,
                          uint16_t fg, uint16_t bg)
{
    if (scale < 1) {
        scale = 1;
    }
    if (ch < 0x20 || ch > 0x7E) {
        ch = ' ';
    }
    const uint8_t *bmp = font_ascii_8x16[ch - 0x20];
    /* 透明背景: 不画底块, 直接叠在背景图上 */
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bmp[row] & (0x80 >> col)) {
                lcd_fill_rect(x + col * scale, y + row * scale,
                              x + col * scale + scale - 1,
                              y + row * scale + scale - 1, fg);
            }
        }
    }
}

void lcd_backlight(bool on)
{
    xl9555_pin_write(XL9555_LCD_BL, on ? 1 : 0);
}
