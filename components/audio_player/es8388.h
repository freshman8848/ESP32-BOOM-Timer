#pragma once

/**
 * @file es8388.h
 * @brief ES8388 音频 CODEC 初始化 (IIC0, 地址 0x10)
 *
 * DNESP32S3 板载 ES8388, 通过 I2S0 (从机) 接收 PCM,
 * LOUT1/ROUT1 驱动板载喇叭与 PHONE 耳机口。
 */

#include "esp_err.h"

esp_err_t es8388_init(void);
