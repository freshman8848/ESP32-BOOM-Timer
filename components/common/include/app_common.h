#pragma once

/**
 * @file app_common.h
 * @brief 应用公共类型与常量
 */
#include <stdint.h>

/* 时/分/秒 时间结构 */
typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} time_hms_t;

/* 工作模式: 倒计时 / 秒表(正计时) */
typedef enum {
    APP_MODE_COUNTDOWN = 0,
    APP_MODE_STOPWATCH,
    APP_MODE_NUM
} app_mode_t;

/* 运行状态 */
typedef enum {
    APP_ST_IDLE = 0,     /* 待机/已清零 */
    APP_ST_SETTING,      /* 设置倒计时时间 */
    APP_ST_RUNNING,      /* 运行中 */
    APP_ST_PAUSED,       /* 已暂停 */
    APP_ST_ALARM,        /* 倒计时结束: LED 常亮 + 蜂鸣持续 */
    APP_ST_DEFUSED,      /* 拆除成功: 炸弹已拆除 */
    APP_ST_NUM
} app_state_t;

/* 倒计时时间范围: 最大 99:59:59 */
#define TIME_MAX_HOURS   99
