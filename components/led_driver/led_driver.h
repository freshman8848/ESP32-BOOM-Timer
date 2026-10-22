#pragma once

/** @file led_driver.h @brief 状态指示 LED (倒计时结束常亮) */

#include <stdbool.h>
#include "esp_err.h"

esp_err_t led_driver_init(void);
void led_set(bool on);
void led_on(void);
void led_off(void);
bool led_is_on(void);
