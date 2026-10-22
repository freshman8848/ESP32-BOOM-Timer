#pragma once

/**
 * @file audio_player.h
 * @brief 板载 ES8388 CODEC + I2S 音效播放 (开机 / 开始计时 / 计时结束)
 *
 * 三个 WAV 音效通过 EMBED_FILES 嵌入固件:
 *   SND_BOOT  - BOOT 按下(开始计时): 01 炸弹已被安放语音
 *   SND_START - 计时中循环: 03 参考滴滴(下包 C4 滴滴背景)
 *   SND_OVER  - 倒计时结束: 02 任务失败语音
 * 播放为后台任务, audio_play() 非阻塞, 正在播放时新请求被忽略。
 */

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    SND_BOOT = 0,    /* BOOT 按下(开始计时): 01 炸弹已被安放语音 */
    SND_START,       /* 计时中循环: 03 参考滴滴(下包 C4 滴滴背景) */
    SND_OVER,        /* 倒计时结束: 02 任务失败语音 */
    SND_SUCCESS,     /* 拆除成功: 04 任务成功语音 (BOOT 长按 3s) */
    SND_EXPLOSION,   /* 倒计时结束: 爆炸音效 (随后接任务失败语音) */
    SND_MAX
} snd_id_t;

/** @brief 初始化 ES8388 + I2S + 后台播放任务 */
esp_err_t audio_init(void);

/** @brief 请求播放一个音效 (单次, 非阻塞; 正在播放循环时先停旧循环) */
esp_err_t audio_play(int id);

/** @brief 循环播放音效 (用于"开始计时"的 C4 滴滴, 直到 audio_stop) */
esp_err_t audio_play_loop(int id);

/** @brief 停止当前循环播放 (打断进行中的写入, 约一个块延时) */
esp_err_t audio_stop(void);
