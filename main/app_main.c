/**
 * @file app_main.c
 * @brief BOOM 简易倒计时计时器 入口
 *
 * 项目需求(项目6)全部实现于 components/app_logic 与各驱动组件:
 *   1. 5 个外部中断按键 (KEY1..KEY5)
 *   2. SPI-LCD 中文显示倒计时数字 + 计时图标
 *   3. 倒计时结束: LED 常亮 + 蜂鸣器持续响, 按 KEY5 清零停止
 *   4. 串口终端控制 (UART0 115200): 模块控制 + LCD 界面切换
 *   5. 代码模块化 (components/ 分层)
 *   6. UART 输出计时启停记录 ([LOG] ...)
 *   7. 最后 3 秒蜂鸣器间断滴滴预警
 *   8. 秒表(正计时)模式, 长按 KEY5 切换
 */
#include <stdio.h>
#include "esp_log.h"
#include "app_logic.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "BOOM countdown timer starting...");
    esp_err_t ret = app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "BOOM ready");
}
