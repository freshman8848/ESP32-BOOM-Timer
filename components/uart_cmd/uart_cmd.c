#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "uart_cmd.h"

static const char *TAG = "uart_cmd";

#define UART_PORT       UART_NUM_0
#define UART_BAUD       115200
#define UART_BUF_SIZE   1024
#define LINE_BUF_SIZE   256

static const uart_cmd_entry_t *s_tbl = NULL;
static int s_tbl_count = 0;
static char s_line[LINE_BUF_SIZE];
static int s_line_len = 0;
static QueueHandle_t s_uart_evt_queue = NULL;

void uart_cmd_print(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

void uart_cmd_printf(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    buf[n] = '\r';
    buf[n + 1] = '\n';
    uart_write_bytes(UART_PORT, buf, n + 2);
}

static void uart_dispatch_line(const char *line)
{
    /* 拆词 */
    char argv_buf[UART_CMD_MAX_TOKENS];
    char *argv[UART_CMD_MAX_ARGS];
    int argc = 0;

    memset(argv_buf, 0, sizeof(argv_buf));
    memset(argv, 0, sizeof(argv));

    const char *p = line;
    char *wb = argv_buf;
    while (*p && argc < UART_CMD_MAX_ARGS) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) {
            break;
        }
        argv[argc++] = wb;
        while (*p && *p != ' ' && *p != '\t' &&
               (size_t)(wb - argv_buf) < sizeof(argv_buf) - 1) {
            *wb++ = *p++;
        }
        *wb++ = '\0';
    }

    if (argc == 0) {
        return;
    }

    for (int i = 0; i < s_tbl_count; i++) {
        if (strcmp(argv[0], s_tbl[i].name) == 0) {
            uart_cmd_printf("> %s", line);
            s_tbl[i].handler(argc, argv);
            return;
        }
    }
    uart_cmd_printf("unknown cmd: %s (type 'help')", argv[0]);
}

static void uart_rx_task(void *arg)
{
    uart_event_t ev;
    for (;;) {
        if (xQueueReceive(s_uart_evt_queue, &ev, portMAX_DELAY) == pdPASS) {
            if (ev.type == UART_DATA) {
                /* 逐字节取, 直到行缓冲收完本次数据 */
                uint8_t b;
                while (uart_read_bytes(UART_PORT, &b, 1, 0) > 0) {
                    if (b == '\n' || b == '\r') {
                        if (s_line_len > 0) {
                            s_line[s_line_len] = '\0';
                            uart_dispatch_line(s_line);
                            s_line_len = 0;
                        }
                    } else if (s_line_len < LINE_BUF_SIZE - 1) {
                        s_line[s_line_len++] = (char)b;
                    } else {
                        s_line_len = 0;   /* 超长丢弃该行 */
                    }
                }
            } else if (ev.type == UART_FIFO_OVF || ev.type == UART_BUFFER_FULL) {
                uart_flush_input(UART_PORT);
            }
        }
    }
}

esp_err_t uart_cmd_init(const uart_cmd_entry_t *tbl, int count)
{
    s_tbl = tbl;
    s_tbl_count = count;

    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t ret = uart_driver_install(UART_PORT, UART_BUF_SIZE * 2,
                                        UART_BUF_SIZE * 2, 16,
                                        &s_uart_evt_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    uart_param_config(UART_PORT, &cfg);
    /* 保持默认引脚 (ESP32-S3: TX=GPIO43, RX=GPIO44, 与烧录共用) */

    static bool task_created = false;
    if (!task_created) {
        BaseType_t ok = xTaskCreate(uart_rx_task, "uart_cmd", 3072, NULL,
                                    6, NULL);
        if (ok != pdPASS) {
            return ESP_ERR_NO_MEM;
        }
        task_created = true;
    }

    uart_cmd_printf("=== BOOM Countdown/Stopwatch Terminal ===");
    uart_cmd_printf("type 'help' for commands");
    ESP_LOGI(TAG, "uart cmd init ok");
    return ESP_OK;
}
