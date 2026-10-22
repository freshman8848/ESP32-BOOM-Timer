#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "buzzer_driver.h"
#include "xl9555.h"
#include "pins.h"

static const char *TAG = "buzzer";

/* 板载蜂鸣器 = XL9555 BEEP (P0_3), 有源蜂鸣器, 低电平响 */

esp_err_t buzzer_driver_init(void)
{
    esp_err_t ret = xl9555_init();
    if (ret != ESP_OK) {
        return ret;
    }
    buzzer_alarm_stop();
    ESP_LOGI(TAG, "buzzer init ok (XL9555 P0_3, active low)");
    return ESP_OK;
}

void buzzer_beep_once(uint32_t ms)
{
    xl9555_pin_write(XL9555_BEEP, 0);
    vTaskDelay(pdMS_TO_TICKS(ms));
    xl9555_pin_write(XL9555_BEEP, 1);
}

void buzzer_alarm_start(void)
{
    xl9555_pin_write(XL9555_BEEP, 0);
}

void buzzer_alarm_stop(void)
{
    xl9555_pin_write(XL9555_BEEP, 1);
}
