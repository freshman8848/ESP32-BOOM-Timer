#pragma once

/**
 * @file uart_cmd.h
 * @brief UART0 串口命令终端 (115200)
 *
 * 应用层向 uart_cmd_init() 注册一张命令表, 串口收到以 \r 或 \n 结尾的
 * 一行后自动按空格拆分为 argc/argv 并调用对应处理函数。
 * 同时提供 uart_cmd_printf() 用于输出提示与 UART 计时启停记录(需求6)。
 */

#include <stdarg.h>
#include "esp_err.h"

#define UART_CMD_MAX_ARGS   8
#define UART_CMD_MAX_TOKENS (UART_CMD_MAX_ARGS * 32)

typedef void (*uart_cmd_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    uart_cmd_handler_t handler;
} uart_cmd_entry_t;

/**
 * @brief 初始化 UART0 (115200) 并启动命令接收任务
 * @param tbl  命令表 (静态存储, 生命周期须与应用一致)
 * @param count 命令条数
 */
esp_err_t uart_cmd_init(const uart_cmd_entry_t *tbl, int count);

/** @brief 向串口格式化输出 (自动追加 \r\n) */
void uart_cmd_printf(const char *fmt, ...);

/** @brief 向串口输出原始字符串 (不追加换行) */
void uart_cmd_print(const char *str);
