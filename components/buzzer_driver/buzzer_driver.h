#pragma once

/**
 * @file buzzer_driver.h
 * @brief 无源蜂鸣器驱动 (LEDC PWM 方波)
 *
 * 提供两种提示方式:
 *   - buzzer_beep_once(): 短暂"滴"一声 (阻塞式, 用于最后 3 秒预警)
 *   - buzzer_alarm_start()/stop(): 持续响 (倒计时结束报警, 直到按清零键)
 */

#include <stdint.h>
#include "esp_err.h"

esp_err_t buzzer_driver_init(void);

/** @brief 短促"滴"一声 (阻塞 ms 毫秒后自动停) */
void buzzer_beep_once(uint32_t ms);

/** @brief 开始持续鸣响 (不阻塞) */
void buzzer_alarm_start(void);

/** @brief 停止鸣响 */
void buzzer_alarm_stop(void);
