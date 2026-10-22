#include <string.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "xl9555.h"
#include "pins.h"

static const char *TAG = "xl9555";

#define I2C_PORT        I2C_NUM_0
#define I2C_SPEED_HZ    400000

/* XL9555 寄存器 */
#define REG_INPUT0      0x00
#define REG_INPUT1      0x01
#define REG_OUTPUT0     0x02
#define REG_OUTPUT1     0x03
#define REG_POL0        0x04
#define REG_POL1        0x05
#define REG_CONF0       0x06   /* 1 = 输入 */
#define REG_CONF1       0x07

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t xl9555_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = { reg, data };
    return i2c_master_transmit(s_dev, buf, 2, -1);
}

static esp_err_t xl9555_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, -1);
}

esp_err_t xl9555_init(void)
{
    if (s_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = I2C_PORT,
            .sda_io_num = PIN_I2C_SDA,
            .scl_io_num = PIN_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2c bus init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    if (s_dev == NULL) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = XL9555_I2C_ADDR,
            .scl_speed_hz = I2C_SPEED_HZ,
        };
        esp_err_t ret = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    /* IO 方向: 1 = 输入。
     * CONF0: P0_0/P0_1 (传感器中断) 输入, 其余输出 -> 0x03
     * CONF1: P1_0..P1_3 输出, P1_4..P1_7 (KEY3..KEY0) 输入 -> 0xF0 */
    esp_err_t ret = xl9555_write_reg(REG_CONF0, 0x03);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "conf0 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = xl9555_write_reg(REG_CONF1, 0xF0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "conf1 failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 上电默认: 蜂鸣器关(高), SPK_EN 静音(高; 低电平才打开功放, 由 audio 初始化打开),
       LCD 复位释放(高), 背光关(低), 输出口其余置 0 */
    xl9555_pin_write(XL9555_BEEP, 1);
    xl9555_pin_write(XL9555_SPK_EN, 1);
    xl9555_pin_write(XL9555_LCD_RST, 1);
    xl9555_pin_write(XL9555_LCD_BL, 0);

    ESP_LOGI(TAG, "XL9555 init ok (addr 0x%02X)", XL9555_I2C_ADDR);
    return ESP_OK;
}

i2c_master_bus_handle_t xl9555_get_bus(void)
{
    return s_bus;
}

int xl9555_pin_read(uint16_t pin)
{
    uint8_t rd[2];
    if (xl9555_read_regs(REG_INPUT0, rd, 2) != ESP_OK) {
        return -1;
    }
    uint16_t ports = (uint16_t)((rd[1] << 8) | rd[0]);
    return (ports & pin) ? 1 : 0;
}

uint16_t xl9555_pin_write(uint16_t pin, int val)
{
    uint8_t rd[2];
    if (xl9555_read_regs(REG_OUTPUT0, rd, 2) != ESP_OK) {
        return 0;
    }
    uint16_t ports = (uint16_t)((rd[1] << 8) | rd[0]);
    if (val) {
        ports |= pin;
    } else {
        ports &= (uint16_t)~pin;
    }
    uint8_t w[2] = { (uint8_t)(ports & 0xFF), (uint8_t)(ports >> 8) };
    xl9555_write_reg(REG_OUTPUT0, w[0]);
    xl9555_write_reg(REG_OUTPUT1, w[1]);
    return ports;
}
