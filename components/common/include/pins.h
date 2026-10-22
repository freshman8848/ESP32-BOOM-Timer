#pragma once

/**
 * @file pins.h
 * @brief 硬件引脚定义 —— 正点原子 ATK-DNESP32S3 (老款开发板)
 *
 * 依据正点原子《ESP32-S3使用指南—IDF版 V1.6》(博客园 zdyz 转载版) 官方例程:
 *   - IIC_EXIO实验: XL9555 IO扩展芯片, IIC0: SDA=IO41, SCL=IO42, 地址 0x20
 *     · BEEP = P0_3 (板载蜂鸣器, 有源, 低电平响)
 *     · KEY0 = P1_7, KEY1 = P1_6, KEY2 = P1_5, KEY3 = P1_4 (按下接 GND)
 *     · SLCD_RST = P1_2 (SPI-LCD 复位), SLCD_PWR = P1_3 (背光, 高电平点亮)
 *   - KEY实验/EXIT实验: BOOT 按键 = GPIO0 (主控直连, 按下接 GND)
 *   - LED实验: LED = IO1 (板载 LED, 低电平点亮)
 *   - SPI_LCD实验: SPI2 (MOSI=IO11, SCLK=IO12, MISO=IO13), LCD_CS=IO21,
 *     LCD_WR(DC)=IO40, 屏幕为 ST7789V (正点原子 SPI LCD 模块,
 *     1.3寸 240x240 / 2.4寸 320x240, 插入底板 WIRELESS 接口)
 *
 * 需求 5 个按键分配: KEY1=KEY0, KEY2=KEY1, KEY3=KEY2, KEY4=KEY3,
 * KEY5=BOOT(GPIO0)。上电/复位时按住 KEY5(BOOT) 会进入下载模式, 属正常现象。
 */

/* ---------------- SPI-LCD (ST7789V) ---------------- */
#define PIN_LCD_SCLK   GPIO_NUM_12
#define PIN_LCD_MOSI   GPIO_NUM_11
#define PIN_LCD_MISO   GPIO_NUM_13
#define PIN_LCD_CS     GPIO_NUM_21
#define PIN_LCD_DC     GPIO_NUM_40   /* WR: 命令/数据 */
/* RST 与背光由 XL9555 控制, 见下 */

/* ---------------- IIC0 (XL9555 扩展芯片) ---------------- */
#define PIN_I2C_SCL    GPIO_NUM_42
#define PIN_I2C_SDA    GPIO_NUM_41
#define XL9555_I2C_ADDR  0x20

/* XL9555 端口位掩码 (16位: P0_0..P0_7 = bit0..7, P1_0..P1_7 = bit8..15) */
#define XL9555_BEEP      0x0008     /* P0_3: 蜂鸣器, 低电平响 */
#define XL9555_SPK_EN    0x0004     /* P0_2: 喇叭功放使能, 低电平开启(官方音乐例程写0打开喇叭) */
#define XL9555_LCD_RST   0x0100     /* P1_2: LCD 复位, 低电平复位 */
#define XL9555_LCD_BL    0x0200     /* P1_3: LCD 背光, 高电平点亮 */
#define XL9555_KEY0      0x8000     /* P1_7 */
#define XL9555_KEY1      0x4000     /* P1_6 */
#define XL9555_KEY2      0x2000     /* P1_5 */
#define XL9555_KEY3      0x1000     /* P1_4 */

/* ---------------- 5 个按键 ---------------- */
/* KEY1..KEY4 = 板载 KEY0..KEY3 (XL9555 轮询读取, 按下=低) */
#define PIN_KEY_EXT1    XL9555_KEY0
#define PIN_KEY_EXT2    XL9555_KEY1
#define PIN_KEY_EXT3    XL9555_KEY2
#define PIN_KEY_EXT4    XL9555_KEY3
/* KEY5 = BOOT 按键 (GPIO0 直连, 按下=低, 内部上拉) */
#define PIN_KEY5        GPIO_NUM_0

/* ---------------- LED 与蜂鸣器 ---------------- */
/* 板载 LED (IO1), 低电平点亮 */
#define PIN_LED         GPIO_NUM_1
/* 蜂鸣器经 XL9555 BEEP (P0_3) 控制, 有源蜂鸣器, 低电平响 */

/* ---------------- ES8388 音频 CODEC (音乐播放器实验) ----------------
 * ES8388 控制走 IIC0 (与 XL9555 共用: SDA=IO41, SCL=IO42), 器件地址 0x10;
 * 音频数据走 I2S0 (从机模式, MCLK = 256 * 采样率):
 *   BCK = IO46, WS = IO9, DAC 数据 = IO10 (ES8388 SDIN), MCLK = IO3
 * 板载喇叭 SPEAKER + PHONE 耳机座输出 (ES8388 LOUT1/ROUT1)。 */
#define ES8388_I2C_ADDR   0x10
#define PIN_I2S_BCK       GPIO_NUM_46
#define PIN_I2S_WS        GPIO_NUM_9
#define PIN_I2S_DO        GPIO_NUM_10
#define PIN_I2S_MCLK      GPIO_NUM_3
