#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "key_driver.h"
#include "xl9555.h"
#include "pins.h"

static const char *TAG = "key";

#define KEY_DEBOUNCE_MS      20
#define KEY_POLL_INTERVAL_MS 10

static QueueHandle_t s_key_queue = NULL;

/* 按键来源 (ATK-DNESP32S3):
 *   KEY_ID_1 = 板载 KEY0 (XL9555 P1_7, IIC 轮询)
 *   KEY_ID_2 = 板载 KEY1 (XL9555 P1_6)
 *   KEY_ID_3 = 板载 KEY2 (XL9555 P1_5)
 *   KEY_ID_4 = 板载 KEY3 (XL9555 P1_4)
 *   KEY_ID_5 = BOOT 按键 (GPIO0, 主控直连, 中断)
 */
static const uint16_t s_key_ext_pin[KEY_COUNT] = {
    [KEY_ID_1] = PIN_KEY_EXT1,
    [KEY_ID_2] = PIN_KEY_EXT2,
    [KEY_ID_3] = PIN_KEY_EXT3,
    [KEY_ID_4] = PIN_KEY_EXT4,
};

static void IRAM_ATTR key_isr_handler(void *arg)
{
    (void)arg;
}

/** @brief 返回某按键当前是否处于"按下"状态 (true=按下) */
static bool key_read_level(key_id_t id)
{
    switch (id) {
    case KEY_ID_1:
    case KEY_ID_2:
    case KEY_ID_3:
    case KEY_ID_4: {
        /* XL9555 板载按键: 按下接 GND, 读 0 */
        int lv = xl9555_pin_read(s_key_ext_pin[id]);
        return (lv == 0);
    }
    case KEY_ID_5:
        return gpio_get_level(PIN_KEY5) == 0;
    default:
        return false;
    }
}

/**
 * 按键扫描任务: 统一轮询 5 键 (BOOT 键另配下降沿中断),
 * 软件防抖 20ms 后确认按下, 按住 >= 800ms 上报长按, 释放上报短按。
 */
static void key_scan_task(void *arg)
{
    bool     pressed[KEY_COUNT];
    bool     long_sent[KEY_COUNT];
    bool     cand[KEY_COUNT];
    uint32_t cand_ms[KEY_COUNT];
    uint32_t press_ms[KEY_COUNT];
    memset(pressed, 0, sizeof(pressed));
    memset(long_sent, 0, sizeof(long_sent));
    memset(cand, 0, sizeof(cand));
    memset(cand_ms, 0, sizeof(cand_ms));
    memset(press_ms, 0, sizeof(press_ms));

    for (;;) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        for (key_id_t id = 0; id < KEY_COUNT; id++) {
            bool level = key_read_level(id);

            if (!pressed[id] && !cand[id] && level) {
                cand[id] = true;
                cand_ms[id] = now;
            } else if (!pressed[id] && cand[id] && level) {
                if ((now - cand_ms[id]) >= KEY_DEBOUNCE_MS) {
                    pressed[id] = true;
                    press_ms[id] = now;
                    long_sent[id] = false;
                    cand[id] = false;
                }
            } else if (!pressed[id] && cand[id] && !level) {
                cand[id] = false;              /* 抖动, 取消本次 */
            } else if (pressed[id] && !level) {
                pressed[id] = false;           /* 释放 */
                if (!long_sent[id]) {
                    key_event_t ev = { .id = id, .action = KEY_ACTION_PRESS };
                    xQueueSend(s_key_queue, &ev, 0);
                }
            } else if (pressed[id] && level && !long_sent[id]) {
                if ((now - press_ms[id]) >= KEY_LONG_PRESS_MS) {
                    key_event_t ev = { .id = id, .action = KEY_ACTION_LONG };
                    xQueueSend(s_key_queue, &ev, 0);
                    long_sent[id] = true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(KEY_POLL_INTERVAL_MS));
    }
}

esp_err_t key_driver_init(void)
{
    if (s_key_queue == NULL) {
        s_key_queue = xQueueCreate(16, sizeof(key_event_t));
    }
    if (s_key_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    /* 扩展按键依赖 XL9555 */
    esp_err_t ret = xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }

    /* BOOT 按键 (GPIO0): 输入 + 上拉 + 下降沿中断 */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_KEY5),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "isr service install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_isr_handler_add(PIN_KEY5, key_isr_handler, NULL);

    static bool task_created = false;
    if (!task_created) {
        BaseType_t ok = xTaskCreatePinnedToCore(key_scan_task, "key_scan",
                                                3072, NULL, 5, NULL, 1);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "task create failed");
            return ESP_ERR_NO_MEM;
        }
        task_created = true;
    }
    ESP_LOGI(TAG, "key driver init ok (KEY0-3=XL9555, KEY5=BOOT/GPIO0)");
    return ESP_OK;
}

bool key_driver_read(key_event_t *ev, TickType_t timeout_ticks)
{
    return xQueueReceive(s_key_queue, ev, timeout_ticks) == pdPASS;
}

bool key_driver_is_pressed(key_id_t id)
{
    if (id >= KEY_COUNT) {
        return false;
    }
    return key_read_level(id);
}
