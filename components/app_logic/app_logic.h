#pragma once

/**
 * @file app_logic.h
 * @brief 简易倒计时计时器 - 业务逻辑入口
 *
 * 初始化所有驱动并启动:
 *   - 心跳任务 (100ms 节拍, 驱动倒计时/秒表/预警/报警)
 *   - 按键事件处理 (KEY1..KEY5)
 *   - 串口命令处理 (help/mode/ui/set/start/pause/reset/status)
 *   - LCD 界面渲染
 */

#include <stdbool.h>
#include "esp_err.h"
#include "app_common.h"

esp_err_t app_init(void);

/* ---------- 供 UI 渲染读取的当前状态 ---------- */
app_mode_t        app_get_mode(void);
app_state_t       app_get_state(void);
const time_hms_t *app_get_countdown(void);
const time_hms_t *app_get_stopwatch(void);
uint8_t           app_get_setting_pos(void);   /* 设置界面调整位: 0=时 1=分 2=秒 */
