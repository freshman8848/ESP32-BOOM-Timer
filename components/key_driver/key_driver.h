#pragma once

/**
 * @file key_driver.h
 * @brief 5 个外部中断按键驱动 (GPIO 中断 + 软件防抖 + 短按/长按)
 *
 * 按键按下(接 GND)触发 GPIO 下降沿中断, 驱动任务做 20ms 防抖确认,
 * 之后:
 *   - 释放时间 < KEY_LONG_PRESS_MS   -> 上报 KEY_ACTION_PRESS (短按)
 *   - 按住时间 >= KEY_LONG_PRESS_MS  -> 上报 KEY_ACTION_LONG  (长按)
 * 事件通过 FreeRTOS 队列发出, 由应用层调用 key_driver_read() 收取。
 */

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define KEY_COUNT           5      /* KEY1..KEY5 */
#define KEY_LONG_PRESS_MS   800

typedef enum {
    KEY_ID_1 = 0,   /* 进入设置时间 */
    KEY_ID_2,       /* 时间 +1 */
    KEY_ID_3,       /* 时间 -1 */
    KEY_ID_4,       /* 开始/暂停 */
    KEY_ID_5,       /* 清零 / 长按切换模式 */
    KEY_ID_MAX
} key_id_t;

typedef enum {
    KEY_ACTION_PRESS = 0,   /* 短按(按下并快速释放) */
    KEY_ACTION_LONG,        /* 长按(按住超过阈值) */
    KEY_ACTION_MAX
} key_action_t;

typedef struct {
    key_id_t     id;
    key_action_t action;
} key_event_t;

/**
 * @brief 初始化按键(注册 GPIO 中断, 创建事件队列与按键任务)
 * @return 0 成功
 */
esp_err_t key_driver_init(void);

/**
 * @brief 从事件队列取一个按键事件 (阻塞, 超时 ticks)
 */
bool key_driver_read(key_event_t *ev, TickType_t timeout_ticks);

/**
 * @brief 查询某按键当前电平 (true=按下, 供状态查询用)
 */
bool key_driver_is_pressed(key_id_t id);
