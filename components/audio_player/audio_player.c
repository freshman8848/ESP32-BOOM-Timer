#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "audio_player.h"
#include "es8388.h"
#include "pins.h"

static const char *TAG = "audio";

#define SAMPLE_RATE  16000
#define BITS         16
#define CH_MONO      1

/* EMBED_FILES 生成的符号: 文件名 snd_*.wav -> _binary_snd_*_wav_start/end */
extern const uint8_t snd_boot_wav_start[]  asm("_binary_snd_boot_wav_start");
extern const uint8_t snd_boot_wav_end[]    asm("_binary_snd_boot_wav_end");
extern const uint8_t snd_start_wav_start[] asm("_binary_snd_start_wav_start");
extern const uint8_t snd_start_wav_end[]   asm("_binary_snd_start_wav_end");
extern const uint8_t snd_over_wav_start[]  asm("_binary_snd_over_wav_start");
extern const uint8_t snd_over_wav_end[]    asm("_binary_snd_over_wav_end");
extern const uint8_t snd_success_wav_start[] asm("_binary_snd_success_wav_start");
extern const uint8_t snd_success_wav_end[]   asm("_binary_snd_success_wav_end");
extern const uint8_t snd_explosion_wav_start[] asm("_binary_snd_explosion_wav_start");
extern const uint8_t snd_explosion_wav_end[]   asm("_binary_snd_explosion_wav_end");

static i2s_chan_handle_t s_tx = NULL;
static QueueHandle_t s_queue = NULL;
static volatile bool s_stop = false;        /* 停止循环播放请求 */
static volatile bool s_loop_playing = false; /* 当前是否处于循环播放 */

/* ---------------- WAV 解析 (RIFF -> 定位 data chunk) ---------------- */

static const uint8_t *wav_find_data(const uint8_t *p, uint32_t len,
                                    uint32_t *data_len)
{
    if (len < 12 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) {
        return NULL;
    }
    uint32_t off = 12;
    while (off + 8 <= len) {
        uint32_t csz = (uint32_t)p[off + 4] | ((uint32_t)p[off + 5] << 8) |
                       ((uint32_t)p[off + 6] << 16) | ((uint32_t)p[off + 7] << 24);
        if (memcmp(p + off, "data", 4) == 0) {
            if (off + 8 + csz > len) {
                csz = len - off - 8;   /* 截断保护 */
            }
            *data_len = csz;
            return p + off + 8;
        }
        off += 8 + csz + (csz & 1);    /* chunk 按 2 字节对齐 */
    }
    return NULL;
}

/* ---------------- 播放 ---------------- */

/* 单声道样本 -> 立体声交织缓冲 (L=R), 适配 ES8388 I2S 立体声帧 */
#define STEREO_BUF_SAMPLES 2048
static int16_t s_stereo_buf[STEREO_BUF_SAMPLES * 2];

static void play_wav(const uint8_t *p, uint32_t len)
{
    uint32_t data_len = 0;
    const uint8_t *d = wav_find_data(p, len, &data_len);
    if (d == NULL) {
        ESP_LOGW(TAG, "bad wav");
        return;
    }
    const int16_t *src = (const int16_t *)d;
    uint32_t n = data_len / 2;               /* 单声道样本数 */

    uint32_t done = 0;
    while (done < n) {
        if (s_stop) {
            ESP_LOGI(TAG, "play interrupted");
            return;
        }
        uint32_t chunk = n - done;
        if (chunk > STEREO_BUF_SAMPLES) {
            chunk = STEREO_BUF_SAMPLES;
        }
        for (uint32_t i = 0; i < chunk; i++) {
            int32_t s = src[done + i];
            s = s * 20 / 100;        /* 音量 20% (约 -14dB), 所有音效统一衰减 */
            s_stereo_buf[i * 2] = (int16_t)s;
            s_stereo_buf[i * 2 + 1] = (int16_t)s;
        }
        size_t written = 0;
        esp_err_t ret = i2s_channel_write(s_tx, s_stereo_buf,
                                          chunk * 4, &written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "i2s write: %s", esp_err_to_name(ret));
            return;
        }
        done += chunk;
    }
    ESP_LOGI(TAG, "play done (n=%lu)", (unsigned long)n);
}

static void play_wav_by_id(int id)
{
    const uint8_t *p = NULL;
    uint32_t len = 0;
    switch (id) {
    case SND_BOOT:
        p = snd_boot_wav_start;
        len = (uint32_t)(snd_boot_wav_end - snd_boot_wav_start);
        break;
    case SND_START:
        p = snd_start_wav_start;
        len = (uint32_t)(snd_start_wav_end - snd_start_wav_start);
        break;
    case SND_OVER:
        p = snd_over_wav_start;
        len = (uint32_t)(snd_over_wav_end - snd_over_wav_start);
        break;
    case SND_SUCCESS:
        p = snd_success_wav_start;
        len = (uint32_t)(snd_success_wav_end - snd_success_wav_start);
        break;
    case SND_EXPLOSION:
        p = snd_explosion_wav_start;
        len = (uint32_t)(snd_explosion_wav_end - snd_explosion_wav_start);
        break;
    default:
        return;
    }
    play_wav(p, len);
}

static void audio_task(void *arg)
{
    int id;
    for (;;) {
        if (xQueueReceive(s_queue, &id, portMAX_DELAY) != pdPASS) {
            continue;
        }
        if (id < 0 || id >= SND_MAX) {
            continue;
        }
        s_stop = false;
        bool loop = (id == SND_START);       /* 开始计时音效: 循环到倒计时结束 */
        ESP_LOGI(TAG, "play start id=%d loop=%d", id, loop);
        do {
            play_wav_by_id(id);
        } while (loop && !s_stop);
        s_loop_playing = false;
        ESP_LOGI(TAG, "play end id=%d", id);
    }
}

/* ---------------- 初始化 ---------------- */

static esp_err_t i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                            I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,  /* S3: PLL_160M */
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCK,
            .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false,
                              .ws_inv = false },
        },
    };
    ret = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init std: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2s_channel_enable(s_tx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "enable: %s", esp_err_to_name(ret));
        return ret;
    }
    i2s_chan_info_t info;
    if (i2s_channel_get_info(s_tx, &info) == ESP_OK) {
        ESP_LOGI(TAG, "I2S0 clk: sclk=%lu mclk=%lu bclk=%lu (expect mclk=4096000)",
                 (unsigned long)info.sclk_hz, (unsigned long)info.mclk_hz,
                 (unsigned long)info.bclk_hz);
    } else {
        ESP_LOGW(TAG, "I2S0 get_info failed");
    }
    ESP_LOGI(TAG, "I2S0 init ok (16kHz stereo, BCK=%d WS=%d DO=%d MCLK=%d)",
             PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DO, PIN_I2S_MCLK);
    return ESP_OK;
}

esp_err_t audio_init(void)
{
    esp_err_t ret = es8388_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2s_init();
    if (ret != ESP_OK) {
        return ret;
    }
    s_queue = xQueueCreate(4, sizeof(int));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(audio_task, "audio", 3072, NULL,
                                            6, NULL, 1);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t audio_play(int id)
{
    if (s_queue == NULL || id < 0 || id >= SND_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "play request id=%d (single)", id);
    xQueueSend(s_queue, &id, 0);
    return ESP_OK;
}

esp_err_t audio_play_loop(int id)
{
    if (s_queue == NULL || id < 0 || id >= SND_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "play request id=%d (loop)", id);
    xQueueSend(s_queue, &id, 0);
    return ESP_OK;
}

esp_err_t audio_stop(void)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_stop = true;          /* 立即打断正在进行的循环/单次播放 */
    s_loop_playing = false;
    /* 清空排队中的待播请求, 防止暂停/清除后残留音效继续播放 */
    int tmp;
    while (xQueueReceive(s_queue, &tmp, 0) == pdPASS) {
    }
    ESP_LOGI(TAG, "stop requested (queue cleared)");
    return ESP_OK;
}
