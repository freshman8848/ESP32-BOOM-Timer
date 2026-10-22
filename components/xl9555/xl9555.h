#pragma once

/**
 * @file xl9555.h
 * @brief XL9555 IO 扩展芯片驱动 (IIC0, 器件地址 0x20)
 *
 * ATK-DNESP32S3 (老款) 板载外设经该芯片控制:
 *   - BEEP    = P0_3  (蜂鸣器, 低电平响, 有源)
 *   - SLCD_RST= P1_2  (SPI-LCD 硬件复位, 低电平复位)
 *   - SLCD_PWR= P1_3  (SPI-LCD 背光, 高电平点亮)
 *   - KEY0    = P1_7  (按键, 按下接 GND)
 *   - KEY1    = P1_6
 *   - KEY2    = P1_5
 *   - KEY3    = P1_4
 * 注意: 板上 24C02 与 XL9555 共用 IIC0, 本项目只用 XL9555。
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/** @brief 初始化 IIC0 总线并配置 XL9555 (按键输入/输出口方向、默认电平) */
esp_err_t xl9555_init(void);

/** @brief 获取 IIC0 总线句柄 (板上 ES8388/24C02 等器件复用同一总线) */
i2c_master_bus_handle_t xl9555_get_bus(void);

/** @brief 读取指定 IO 电平 (0/1), pin 取值见 pins.h 的 XL9555_* 宏; 失败返回 -1 */
int xl9555_pin_read(uint16_t pin);

/** @brief 输出指定 IO 电平, 返回写入后的 16 位输出状态 */
uint16_t xl9555_pin_write(uint16_t pin, int val);
