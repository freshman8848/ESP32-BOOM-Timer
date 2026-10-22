#include <stdio.h>
#include <string.h>
#include "app_ui.h"
#include "app_logic.h"
#include "lcd_driver.h"

/* C4 背景图 (320x240 RGB565 大端, 嵌入 flash, 经 EMBED_FILES) */
extern const uint8_t bg_image_bin_start[] asm("_binary_bg_image_bin_start");
extern const uint8_t bg_image_bin_end[]   asm("_binary_bg_image_bin_end");

static const uint8_t *bg_image(void)
{
    return bg_image_bin_start;
}

/* ---------------- 配色 ---------------- */
#define BG_COLOR      LCD_COLOR(8, 12, 60)      /* 深蓝背景 */
#define TITLE_COLOR   LCD_COLOR(0, 255, 255)    /* 青色标题 */
#define ICON_COLOR    LCD_COLOR(0, 255, 255)
#define NUM_IDLE      LCD_COLOR(255, 255, 255)  /* 数字: 运行/空闲 白 */
#define NUM_PAUSED    LCD_COLOR(255, 255, 0)    /* 数字: 暂停 黄 */
#define NUM_ALARM     LCD_COLOR(255, 0, 0)      /* 数字: 结束 红 */
#define NUM_SETTING   LCD_COLOR(0, 255, 255)    /* 数字: 设置 青 */
#define ST_OK         LCD_COLOR(0, 255, 0)      /* 状态: 运行 绿 */
#define ST_PAUSE      LCD_COLOR(255, 255, 0)    /* 状态: 暂停 黄 */
#define ST_OVER       LCD_COLOR(255, 0, 0)      /* 状态: 结束 红 */
#define ST_IDLE_C     LCD_COLOR(160, 160, 160)  /* 状态: 空闲 灰 */
#define HINT_COLOR    LCD_COLOR(150, 150, 200)

/* ---------------- 横屏 320x240 布局 ---------------- */
#define TITLE_Y       8
#define ICON_CX       44
#define ICON_CY       24
#define ICON_R        12
#define TITLE_X       64

#define DIGIT_Y       78
#define DIGIT_SCALE   4                 /* 大数字放大倍数 (8*4=32px/字符) */
#define SEG_W         (2 * 8 * DIGIT_SCALE)   /* 每段(时/分/秒)宽度 64 */
#define COLON_W       (8 * DIGIT_SCALE)       /* 冒号宽度 32 */

#define UNIT_Y        158               /* 设置界面 时分秒 单位标注 */
#define STATE_Y       178
#define HINT_Y        214

static uint16_t digit_color(void)
{
    switch (app_get_state()) {
    case APP_ST_RUNNING:  return NUM_IDLE;
    case APP_ST_PAUSED:   return NUM_PAUSED;
    case APP_ST_ALARM:    return NUM_ALARM;
    case APP_ST_SETTING:  return NUM_SETTING;
    case APP_ST_DEFUSED:  return ST_OK;
    default:              return NUM_IDLE;
    }
}

static void draw_clock_icon(void)
{
    int cx = ICON_CX, cy = ICON_CY;
    lcd_draw_circle(cx, cy, ICON_R, ICON_COLOR);
    /* 时针指向 12 点, 分针指向 3 点 */
    lcd_draw_line(cx, cy, cx, cy - ICON_R + 3, ICON_COLOR);
    lcd_draw_line(cx, cy, cx + ICON_R - 4, cy - 4, ICON_COLOR);
}

static void draw_title(void)
{
    const char *title = "倒计时";
    if (app_get_mode() == APP_MODE_STOPWATCH) {
        title = "秒表";
    }
    if (app_get_state() == APP_ST_SETTING) {
        title = "设置";
    }
    draw_clock_icon();
    lcd_draw_string(TITLE_X, TITLE_Y, title, TITLE_COLOR, BG_COLOR);
}

/**
 * 显示 HH:MM:SS 或 MM:SS (放大 4 倍)。
 * 设置界面无闪烁, 仅以单位标注提示 时/分/秒。
 */
static void draw_time(void)
{
    char buf[16];
    bool setting = (app_get_state() == APP_ST_SETTING);
    const time_hms_t *t;
    int chars, x0;
    uint16_t fg = digit_color();

    if (app_get_state() == APP_ST_DEFUSED) {
        return;    /* 拆除成功: 全屏大字替代数字区 */
    }

    if (app_get_mode() == APP_MODE_COUNTDOWN) {
        t = app_get_countdown();
    } else {
        t = app_get_stopwatch();
    }

    if (t->hour > 0 || setting) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->hour, t->min, t->sec);
        chars = 8;
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", t->min, t->sec);
        chars = 5;
    }
    x0 = (LCD_WIDTH - chars * 8 * DIGIT_SCALE) / 2;

    int x = x0;
    for (int i = 0; buf[i]; i++) {
        lcd_draw_char_scaled(x, DIGIT_Y, buf[i], DIGIT_SCALE, fg, BG_COLOR);
        x += 8 * DIGIT_SCALE;
    }

    /* 设置界面: 在数字段下方标注 时/分/秒, 高亮当前调整位 (KEY0 切换) */
    if (setting) {
        const char *units[] = { "时", "分", "秒" };
        uint8_t pos = app_get_setting_pos();   /* 0=时 1=分 2=秒 */
        int ux = x0;
        for (int i = 0; i < 3; i++) {
            int ucx = ux + SEG_W / 2 - 8;    /* 汉字 16 宽, 居中 */
            lcd_draw_string(ucx, UNIT_Y, units[i],
                            (i == pos) ? NUM_SETTING : LCD_COLOR_DGRAY, BG_COLOR);
            ux += SEG_W + COLON_W;
        }
    }
}

static void draw_state(void)
{
    if (app_get_state() == APP_ST_SETTING) {
        return;    /* 设置界面的单位标注已绘制, 状态行不再重复 */
    }
    if (app_get_state() == APP_ST_DEFUSED) {
        /* 拆除成功: "炸弹已拆除" 4 倍放大铺满全屏 (5*64=320 = 满宽) */
        const char *s = "炸弹已拆除";
        int scale = 4;
        int y = (LCD_HEIGHT - 16 * scale) / 2;   /* 垂直居中 */
        int x = 0;                                /* 满宽 */
        for (const char *p = s; *p; p += 3) {
            uint16_t code = ((unsigned char)p[0] & 0x0F) << 12 |
                            ((unsigned char)p[1] & 0x3F) << 6 |
                            ((unsigned char)p[2] & 0x3F);
            lcd_draw_hz_scaled(x, y, code, scale, ST_OK, BG_COLOR);
            x += 16 * scale;
        }
        return;
    }
    const char *s = NULL;
    uint16_t c = ST_IDLE_C;
    switch (app_get_state()) {
    case APP_ST_IDLE:     s = "空闲"; c = ST_IDLE_C; break;
    case APP_ST_RUNNING:  s = "运行"; c = ST_OK;     break;
    case APP_ST_PAUSED:   s = "暂停"; c = ST_PAUSE;  break;
    case APP_ST_ALARM:    s = "已结束"; c = ST_OVER; break;
    default:              break;
    }
    if (s == NULL) {
        return;
    }
    int n = 0;
    for (const char *p = s; *p; p++) {
        n += ((unsigned char)*p >= 0x80) ? 16 : 8;
    }
    lcd_draw_string((LCD_WIDTH - n) / 2, STATE_Y, s, c, BG_COLOR);
}

static void draw_hint(void)
{
    const char *h = NULL;
    switch (app_get_state()) {
    case APP_ST_IDLE:
        h = (app_get_mode() == APP_MODE_COUNTDOWN)
                ? "K0:Set  BOOT:Start  K3:Clear"
                : "BOOT:Start  K0:Set  K3:Clear";
        break;
    case APP_ST_RUNNING:
        h = "BOOT:Pause  K3:Clear";
        break;
    case APP_ST_PAUSED:
        h = "BOOT:Resume  K3:Clear";
        break;
    case APP_ST_ALARM:
        h = "BOOT:Clear";
        break;
    case APP_ST_DEFUSED:
        h = "BOOT:Clear";
        break;
    case APP_ST_SETTING:
        h = "K0:Switch K1:+1 K2:-1 K3:Save";
        break;
    default:
        break;
    }
    if (h == NULL) {
        return;
    }
    int w = 0;
    for (const char *p = h; *p; p++) {
        w += 8;
    }
    lcd_draw_string((LCD_WIDTH - w) / 2, HINT_Y, h, HINT_COLOR, BG_COLOR);
}

void ui_render(void)
{
    static bool s_first = true;
    static app_state_t s_last_state = (app_state_t)-1;
    static app_mode_t  s_last_mode = (app_mode_t)-1;
    static time_hms_t  s_last_time = { 0, 0, 0 };
    static uint8_t     s_last_set_pos = 0xFF;

    app_state_t st = app_get_state();
    app_mode_t  mo = app_get_mode();
    const time_hms_t *t = (mo == APP_MODE_COUNTDOWN) ? app_get_countdown()
                                                     : app_get_stopwatch();
    uint8_t set_pos = (st == APP_ST_SETTING) ? app_get_setting_pos() : 0xFF;
    bool set_pos_changed = (set_pos != s_last_set_pos);

    if (s_first || st != s_last_state || mo != s_last_mode) {
        /* 界面/状态切换: 整屏重绘 (C4 背景 + 文字) */
        lcd_draw_rgb565(0, 0, LCD_WIDTH, LCD_HEIGHT, bg_image());
        draw_title();
        draw_time();
        draw_state();
        draw_hint();
        s_first = false;
    } else if (set_pos_changed || memcmp(t, &s_last_time, sizeof(time_hms_t)) != 0) {
        /* 仅数字/调整位变化(走秒/设置加减/切换位): 从背景图恢复数字区, 再重绘 */
        int y0 = DIGIT_Y - 4;
        int y1 = UNIT_Y + 16 + 2;
        lcd_draw_rgb565(0, y0, LCD_WIDTH, y1 - y0 + 1,
                        bg_image() + (size_t)y0 * LCD_WIDTH * 2);
        draw_time();
    }

    s_last_state = st;
    s_last_mode  = mo;
    s_last_time  = *t;
    s_last_set_pos = set_pos;
}

void ui_init(void)
{
    ui_render();
}
