#include "driver/gpio.h"
#include "esp_log.h"
#include "led_driver.h"
#include "pins.h"

static const char *TAG = "led";
static bool s_on = false;

esp_err_t led_driver_init(void)
{
    /* 板载 LED = IO1, 低电平点亮 */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    led_off();
    ESP_LOGI(TAG, "led init ok (IO1, active low)");
    return ESP_OK;
}

void led_set(bool on)
{
    s_on = on;
    gpio_set_level(PIN_LED, on ? 0 : 1);
}

void led_on(void)
{
    led_set(true);
}

void led_off(void)
{
    led_set(false);
}

bool led_is_on(void)
{
    return s_on;
}
