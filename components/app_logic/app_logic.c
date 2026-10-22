#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_logic.h"
#include "app_ui.h"
#include "key_driver.h"
#include "lcd_driver.h"
#include "led_driver.h"
#include "buzzer_driver.h"
#include "uart_cmd.h"
#include "audio_player.h"

static const char *TAG = "app";

/* ==================== 应用状态 ==================== */

static app_mode_t  s_mode = APP_MODE_COUNTDOWN;
static app_state_t s_state = APP_ST_IDLE;
static time_hms_t  s_countdown = { 0, 1, 0 };   /* 倒计时设定/剩余, 默认 01:00 */
static time_hms_t  s_stopwatch = { 0, 0, 0 };   /* 秒表累计 */
static bool s_ui_dirty = true;
static uint8_t s_set_pos = 1;        /* 设置界面调整位: 0=时 1=分 2=秒 (KEY0 切换) */
static uint32_t s_boot_hold = 0;     /* BOOT 按住累计 tick 数 (100ms/tick, 30 = 3s) */

/* ==================== 事件通道 ==================== */

typedef enum {
    APP_EV_TICK_SEC = 0,   /* 每秒节拍 */
    APP_EV_TICK_BLINK,     /* 500ms 闪烁节拍 */
    APP_EV_KEY,            /* a = key_id<<1 | action */
    APP_EV_CMD,            /* a = 命令码, b = 参数 */
} app_ev_type_t;

typedef enum {
    APP_CMD_MODE_COUNTDOWN = 0,
    APP_CMD_MODE_STOPWATCH,
    APP_CMD_UI_HOME,
    APP_CMD_UI_SETTINGS,
    APP_CMD_SET_TIME,
    APP_CMD_START,
    APP_CMD_PAUSE,
    APP_CMD_RESET,
    APP_CMD_STATUS,
} app_cmd_t;

typedef struct {
    app_ev_type_t type;
    int32_t a;
    int32_t b;
} app_ev_t;

static QueueHandle_t s_app_queue = NULL;

/* ==================== 工具函数 ==================== */

static uint32_t hms_to_sec(const time_hms_t *t)
{
    return (uint32_t)t->hour * 3600 + t->min * 60 + t->sec;
}

static void sec_to_hms(uint32_t sec, time_hms_t *t)
{
    t->hour = (uint8_t)(sec / 3600);
    t->min  = (uint8_t)((sec % 3600) / 60);
    t->sec  = (uint8_t)(sec % 60);
}

static const char *mode_name(void)
{
    return s_mode == APP_MODE_COUNTDOWN ? "countdown" : "stopwatch";
}

static const char *state_name(void)
{
    switch (s_state) {
    case APP_ST_IDLE:     return "idle";
    case APP_ST_SETTING:  return "setting";
    case APP_ST_RUNNING:  return "running";
    case APP_ST_PAUSED:   return "paused";
    case APP_ST_ALARM:    return "alarm";
    case APP_ST_DEFUSED:  return "defused";
    default:              return "?";
    }
}

/** 需求6: UART 输出计时启停记录 */
static void log_evt(const char *action, const char *detail)
{
    uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000);
    uart_cmd_printf("[LOG] %s %s %s @%lus", mode_name(), action, detail, up);
}

/* ==================== 状态变更 ==================== */

static void set_ui_dirty(void)
{
    s_ui_dirty = true;
}

static void set_inc(int delta)
{
    /* 按当前调整位增减 (0=时 1=分 2=秒), 各自循环 */
    switch (s_set_pos) {
    case 0: {                        /* 时: 0..99 循环 */
        int h = (int)s_countdown.hour + delta;
        if (h < 0) {
            h = 99;
        }
        if (h > 99) {
            h = 0;
        }
        s_countdown.hour = (uint8_t)h;
        break;
    }
    case 1:                          /* 分: 0..59 循环 */
        s_countdown.min = (uint8_t)((s_countdown.min + delta + 60) % 60);
        break;
    default:                         /* 秒: 0..59 循环 */
        s_countdown.sec = (uint8_t)((s_countdown.sec + delta + 60) % 60);
        break;
    }
    set_ui_dirty();
}

static void switch_mode(app_mode_t m)
{
    if (s_state != APP_ST_IDLE) {
        uart_cmd_printf("mode switch only allowed in idle state");
        return;
    }
    s_mode = m;
    s_stopwatch.hour = 0;
    s_stopwatch.min = 0;
    s_stopwatch.sec = 0;
    log_evt("MODE", s_mode == APP_MODE_COUNTDOWN ? "countdown" : "stopwatch");
    set_ui_dirty();
}

static void enter_setting(void)
{
    if (s_mode != APP_MODE_COUNTDOWN || s_state != APP_ST_IDLE) {
        return;
    }
    s_state = APP_ST_SETTING;
    s_set_pos = 1;                   /* 进入设置默认调整"分" */
    set_ui_dirty();
}

static void leave_setting(bool save)
{
    if (s_state != APP_ST_SETTING) {
        return;
    }
    s_state = APP_ST_IDLE;
    if (save) {
        char d[16];
        snprintf(d, sizeof(d), "%02d:%02d:%02d", s_countdown.hour,
                 s_countdown.min, s_countdown.sec);
        log_evt("SET", d);
    }
    set_ui_dirty();
}

static void do_start(void)
{
    if (s_mode == APP_MODE_COUNTDOWN) {
        if (s_state == APP_ST_IDLE) {
            if (hms_to_sec(&s_countdown) == 0) {
                uart_cmd_printf("countdown time is zero, set it first");
                return;
            }
            s_state = APP_ST_RUNNING;
            log_evt("START", "countdown");
            audio_play(SND_BOOT);            /* 01 炸弹已被安放语音 (单次, 播完自动接滴滴循环) */
            audio_play_loop(SND_START);      /* 03 参考滴滴(下包滴滴)循环到倒计时结束 */
        } else if (s_state == APP_ST_PAUSED) {
            s_state = APP_ST_RUNNING;
            log_evt("RESUME", "countdown");
            audio_play_loop(SND_START);      /* 恢复后继续循环 */
        }
    } else {
        if (s_state == APP_ST_IDLE) {
            s_state = APP_ST_RUNNING;
            log_evt("START", "stopwatch");
            audio_play(SND_BOOT);            /* 01 炸弹已被安放语音 */
            audio_play_loop(SND_START);      /* 03 参考滴滴循环 (暂停时停止) */
        } else if (s_state == APP_ST_PAUSED) {
            s_state = APP_ST_RUNNING;
            log_evt("RESUME", "stopwatch");
            audio_play_loop(SND_START);
        }
    }
    set_ui_dirty();
}

static void do_pause(void)
{
    if (s_state != APP_ST_RUNNING) {
        return;
    }
    s_state = APP_ST_PAUSED;
    audio_stop();                            /* 暂停: 停止循环音效 */
    if (s_mode == APP_MODE_COUNTDOWN) {
        char d[24];
        snprintf(d, sizeof(d), "remain=%02d:%02d:%02d", s_countdown.hour,
                 s_countdown.min, s_countdown.sec);
        log_evt("PAUSED", d);
    } else {
        char d[24];
        snprintf(d, sizeof(d), "elapsed=%02d:%02d:%02d", s_stopwatch.hour,
                 s_stopwatch.min, s_stopwatch.sec);
        log_evt("PAUSED", d);
    }
    set_ui_dirty();
}

static void do_reset(void)
{
    audio_stop();                            /* 清除: 停止循环音效 */
    if (s_mode == APP_MODE_COUNTDOWN) {
        if (s_state == APP_ST_ALARM) {
            led_off();
            buzzer_alarm_stop();
            log_evt("CLEAR", "alarm stopped");
        } else {
            log_evt("RESET", "countdown cleared");
        }
        s_countdown.hour = 0;
        s_countdown.min = 0;
        s_countdown.sec = 0;
    } else {
        log_evt("RESET", "stopwatch cleared");
        s_stopwatch.hour = 0;
        s_stopwatch.min = 0;
        s_stopwatch.sec = 0;
    }
    s_state = APP_ST_IDLE;
    set_ui_dirty();
}

/* 拆除成功: BOOT 长按 3s (倒计时运行中) */
static void do_defuse(void)
{
    s_countdown.hour = 0;
    s_countdown.min = 0;
    s_countdown.sec = 0;
    s_state = APP_ST_DEFUSED;
    audio_stop();                    /* 停滴滴循环 */
    audio_play(SND_SUCCESS);         /* 04 任务成功语音 */
    log_evt("DEFUSED", "bomb removed");
    set_ui_dirty();
}

/* ==================== 事件处理 ==================== */

static void on_second(void)
{
    /* BOOT 长按 3s (100ms/tick 计 30 次): 倒计时运行中拆除炸弹 */
    if (s_state == APP_ST_RUNNING && key_driver_is_pressed(KEY_ID_5)) {
        if (++s_boot_hold >= 30) {
            s_boot_hold = 0;
            do_defuse();
        }
    } else {
        s_boot_hold = 0;
    }

    if (s_mode == APP_MODE_COUNTDOWN && s_state == APP_ST_RUNNING) {
        uint32_t sec = hms_to_sec(&s_countdown);
        if (sec > 0) {
            sec--;
            sec_to_hms(sec, &s_countdown);
            if (sec == 0) {
                /* 倒计时结束: LED 常亮 + 爆炸音效 + 02 任务失败语音 */
                s_state = APP_ST_ALARM;
                audio_stop();                  /* 先停滴滴循环音效 */
                led_on();
                audio_play(SND_EXPLOSION);     /* 爆炸音效 (3s) */
                audio_play(SND_OVER);          /* 02: 任务失败语音 (随后播放) */
                log_evt("ALARM", "time up");
            } else if (sec <= 3) {
                /* 需求7: 最后 3 秒, 蜂鸣器间断滴滴预警 */
                buzzer_beep_once(80);
            }
            set_ui_dirty();
        }
    } else if (s_mode == APP_MODE_STOPWATCH && s_state == APP_ST_RUNNING) {
        uint32_t sec = hms_to_sec(&s_stopwatch) + 1;
        if (sec >= 100 * 3600) {
            sec = 0;
        }
        sec_to_hms(sec, &s_stopwatch);
        set_ui_dirty();
    }
}

static void on_key(int key_id, int action)
{
    if (s_state == APP_ST_SETTING) {
        switch (key_id) {
        case KEY_ID_1:                       /* KEY0: 切换调整位 (时/分/秒 循环) */
            s_set_pos = (uint8_t)((s_set_pos + 1) % 3);
            set_ui_dirty();
            break;
        case KEY_ID_2:                       /* KEY1: 当前位 +1 秒 */
            set_inc(1);
            break;
        case KEY_ID_3:                       /* KEY2: 当前位 -1 秒 */
            set_inc(-1);
            break;
        case KEY_ID_4:                       /* KEY3: 保存退出 */
            leave_setting(true);
            break;
        case KEY_ID_5:                       /* 取消退出 */
            leave_setting(false);
            break;
        default:
            break;
        }
        return;
    }

    if (action == KEY_ACTION_LONG) {
        /* BOOT 长按已用于"3 秒拆除"功能, 不再切换工作模式 */
        return;
    }

    switch (key_id) {
    case KEY_ID_1:                           /* KEY0: 进入设置时间 */
        enter_setting();
        break;
    case KEY_ID_4:                           /* KEY3: 清除计数 */
        do_reset();                          /* 运行/暂停/报警时清零回待机 */
        break;
    case KEY_ID_5:                           /* BOOT: 开始/暂停/停止/清除 */
        if (s_state == APP_ST_ALARM || s_state == APP_ST_DEFUSED) {
            do_reset();                      /* 报警/拆除完成: 清除回待机 */
        } else if (s_state == APP_ST_IDLE || s_state == APP_ST_PAUSED) {
            do_start();                      /* 待机开始 / 暂停继续 */
        } else if (s_state == APP_ST_RUNNING) {
            do_pause();                      /* 运行暂停 */
        }
        break;
    default:
        break;
    }
}

static void on_cmd(int cmd, int32_t param)
{
    switch ((app_cmd_t)cmd) {
    case APP_CMD_MODE_COUNTDOWN:
        switch_mode(APP_MODE_COUNTDOWN);
        break;
    case APP_CMD_MODE_STOPWATCH:
        switch_mode(APP_MODE_STOPWATCH);
        break;
    case APP_CMD_UI_HOME:
        if (s_state == APP_ST_SETTING) {
            leave_setting(false);
        }
        break;
    case APP_CMD_UI_SETTINGS:
        enter_setting();
        break;
    case APP_CMD_SET_TIME:
        if (s_state != APP_ST_IDLE || s_mode != APP_MODE_COUNTDOWN) {
            uart_cmd_printf("set allowed only in idle countdown mode");
            break;
        }
        sec_to_hms((uint32_t)param, &s_countdown);
        set_ui_dirty();
        break;
    case APP_CMD_START:
        do_start();
        break;
    case APP_CMD_PAUSE:
        do_pause();
        break;
    case APP_CMD_RESET:
        do_reset();
        break;
    case APP_CMD_STATUS:
        uart_cmd_printf("mode=%s state=%s", mode_name(), state_name());
        uart_cmd_printf("countdown=%02d:%02d:%02d stopwatch=%02d:%02d:%02d",
                        s_countdown.hour, s_countdown.min, s_countdown.sec,
                        s_stopwatch.hour, s_stopwatch.min, s_stopwatch.sec);
        break;
    default:
        break;
    }
}

static void app_task(void *arg)
{
    app_ev_t ev;
    for (;;) {
        if (xQueueReceive(s_app_queue, &ev, portMAX_DELAY) == pdPASS) {
            switch (ev.type) {
            case APP_EV_TICK_SEC:
                on_second();
                break;
            case APP_EV_TICK_BLINK:
                if (s_state == APP_ST_SETTING) {
                    /* 按住 KEY1/KEY2 自动连续 +1/-1 秒 (500ms 重复) */
                    if (key_driver_is_pressed(KEY_ID_2)) {
                        set_inc(1);
                    }
                    if (key_driver_is_pressed(KEY_ID_3)) {
                        set_inc(-1);
                    }
                }
                break;
            case APP_EV_KEY:
                on_key((int)ev.a >> 1, (int)ev.a & 1);
                break;
            case APP_EV_CMD:
                on_cmd((int)ev.a, ev.b);
                break;
            default:
                break;
            }
            if (s_ui_dirty) {
                ui_render();
                s_ui_dirty = false;
            }
        }
    }
}

/* ==================== 心跳/按键转发任务 ==================== */

static void tick_task(void *arg)
{
    int cnt = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        cnt++;
        app_ev_t ev = { .type = APP_EV_TICK_SEC, .a = 0, .b = 0 };
        xQueueSend(s_app_queue, &ev, 0);
        if (cnt % 5 == 0) {                 /* 500ms */
            app_ev_t evb = { .type = APP_EV_TICK_BLINK, .a = 0, .b = 0 };
            xQueueSend(s_app_queue, &evb, 0);
        }
    }
}

static void key_task(void *arg)
{
    key_event_t kev;
    for (;;) {
        if (key_driver_read(&kev, portMAX_DELAY)) {
            app_ev_t ev = {
                .type = APP_EV_KEY,
                .a = (int32_t)((int)kev.id << 1) | (int)kev.action,
                .b = 0,
            };
            xQueueSend(s_app_queue, &ev, 0);
        }
    }
}

/* ==================== 串口命令 ==================== */

static void cmd_help(int argc, char **argv)
{
    uart_cmd_printf("commands:");
    uart_cmd_printf("  mode countdown|stopwatch  - switch mode (idle only)");
    uart_cmd_printf("  ui home|settings         - switch UI page");
    uart_cmd_printf("  set <HH:MM:SS>|<secs>    - set countdown time (idle)");
    uart_cmd_printf("  start | pause | reset    - control timer");
    uart_cmd_printf("  status                   - show state");
}

static void cmd_mode(int argc, char **argv)
{
    if (argc < 2) {
        uart_cmd_printf("usage: mode countdown|stopwatch");
        return;
    }
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_STATUS, .b = 0 };
    if (strcmp(argv[1], "countdown") == 0) {
        ev.a = APP_CMD_MODE_COUNTDOWN;
    } else if (strcmp(argv[1], "stopwatch") == 0) {
        ev.a = APP_CMD_MODE_STOPWATCH;
    } else {
        uart_cmd_printf("unknown mode: %s", argv[1]);
        return;
    }
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_ui(int argc, char **argv)
{
    if (argc < 2) {
        uart_cmd_printf("usage: ui home|settings");
        return;
    }
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_STATUS, .b = 0 };
    if (strcmp(argv[1], "home") == 0) {
        ev.a = APP_CMD_UI_HOME;
    } else if (strcmp(argv[1], "settings") == 0) {
        ev.a = APP_CMD_UI_SETTINGS;
    } else {
        uart_cmd_printf("unknown page: %s", argv[1]);
        return;
    }
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_set(int argc, char **argv)
{
    if (argc < 2) {
        uart_cmd_printf("usage: set <HH:MM:SS>|<secs>");
        return;
    }
    uint32_t sec = 0;
    if (strchr(argv[1], ':')) {
        unsigned h = 0, m = 0, s = 0;
        if (sscanf(argv[1], "%u:%u:%u", &h, &m, &s) == 3) {
            if (h <= 99 && m <= 59 && s <= 59) {
                sec = h * 3600 + m * 60 + s;
            }
        }
    } else {
        sec = (uint32_t)atoi(argv[1]);
    }
    if (sec == 0 || sec >= 100 * 3600) {
        uart_cmd_printf("invalid time (1..99:59:59)");
        return;
    }
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_SET_TIME, .b = (int32_t)sec };
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_start(int argc, char **argv)
{
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_START, .b = 0 };
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_pause(int argc, char **argv)
{
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_PAUSE, .b = 0 };
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_reset(int argc, char **argv)
{
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_RESET, .b = 0 };
    xQueueSend(s_app_queue, &ev, 0);
}

static void cmd_status(int argc, char **argv)
{
    app_ev_t ev = { .type = APP_EV_CMD, .a = APP_CMD_STATUS, .b = 0 };
    xQueueSend(s_app_queue, &ev, 0);
}

static const uart_cmd_entry_t s_cmd_tbl[] = {
    { "help",   "show this help",                 cmd_help   },
    { "mode",   "switch mode",                    cmd_mode   },
    { "ui",     "switch UI page",                 cmd_ui     },
    { "set",    "set countdown time",             cmd_set    },
    { "start",  "start/resume timer",             cmd_start  },
    { "pause",  "pause timer",                    cmd_pause  },
    { "reset",  "clear timer / stop alarm",       cmd_reset  },
    { "status", "show state",                     cmd_status },
};

/* ==================== 对外接口 ==================== */

esp_err_t app_init(void)
{
    s_app_queue = xQueueCreate(32, sizeof(app_ev_t));
    if (s_app_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = key_driver_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = led_driver_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = buzzer_driver_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = lcd_driver_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = audio_init();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = uart_cmd_init(s_cmd_tbl,
                        sizeof(s_cmd_tbl) / sizeof(s_cmd_tbl[0]));
    if (ret != ESP_OK) {
        return ret;
    }

    ui_init();
    /* 01 开局音效改为按 BOOT 开始计时时播放 (见 do_start), 开机不再播放 */

    xTaskCreatePinnedToCore(app_task, "app_task", 4096, NULL, 7, NULL, 1);
    xTaskCreatePinnedToCore(tick_task, "app_tick", 2048, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(key_task, "app_key", 2048, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "app init ok (BOOM countdown timer)");
    return ESP_OK;
}

/* ---------- UI 状态读取 ---------- */

app_mode_t app_get_mode(void)
{
    return s_mode;
}

app_state_t app_get_state(void)
{
    return s_state;
}

const time_hms_t *app_get_countdown(void)
{
    return &s_countdown;
}

const time_hms_t *app_get_stopwatch(void)
{
    return &s_stopwatch;
}

uint8_t app_get_setting_pos(void)
{
    return s_set_pos;
}
