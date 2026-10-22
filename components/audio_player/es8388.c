#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "es8388.h"
#include "xl9555.h"
#include "pins.h"

static const char *TAG = "es8388";

#define I2C_SPEED_HZ 400000

static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t es8388_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    esp_err_t ret = i2c_master_transmit(s_dev, buf, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write reg 0x%02X=0x%02X failed: %s",
                 reg, val, esp_err_to_name(ret));
    }
    return ret;
}

static uint8_t es8388_read_reg(uint8_t reg)
{
    uint8_t v = 0;
    if (i2c_master_transmit_receive(s_dev, &reg, 1, &v, 1, -1) != ESP_OK) {
        return 0;
    }
    return v;
}

/* 按正点原子《ESP32-S3使用指南—IDF版》第四十一章官方 es8388_init 序列 */
esp_err_t es8388_init(void)
{
    if (s_dev == NULL) {
        i2c_master_bus_handle_t bus = xl9555_get_bus();
        if (bus == NULL) {
            ESP_LOGE(TAG, "i2c bus not ready (call xl9555_init first)");
            return ESP_ERR_INVALID_STATE;
        }
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ES8388_I2C_ADDR,
            .scl_speed_hz = I2C_SPEED_HZ,
        };
        esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &s_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    esp_err_t ret = ESP_OK;
    /* 软复位 */
    ret |= es8388_write_reg(0x00, 0x80);
    ret |= es8388_write_reg(0x00, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));          /* 等待复位完成 */
    /* 模拟部分: 先整体掉电再恢复 */
    ret |= es8388_write_reg(0x01, 0x58);
    ret |= es8388_write_reg(0x01, 0x50);
    /* 数字部分: 先掉电再恢复 */
    ret |= es8388_write_reg(0x02, 0xF3);
    ret |= es8388_write_reg(0x02, 0xF0);
    /* 麦克风偏置电源关闭 */
    ret |= es8388_write_reg(0x03, 0x09);
    /* VMID 500K 驱动使能 */
    ret |= es8388_write_reg(0x00, 0x06);
    /* DAC 电源管理, 初始化阶段不开输出通道 */
    ret |= es8388_write_reg(0x04, 0x00);
    /* MCLK 不分频, 从机模式 */
    ret |= es8388_write_reg(0x08, 0x00);
    /* DAC LRCK 与 ADC LRCK 共用 */
    ret |= es8388_write_reg(0x2B, 0x80);
    /* ADC L/R PGA 增益 +24dB (录音用, 播放无影响) */
    ret |= es8388_write_reg(0x09, 0x88);
    /* ADC 数据 16bit I2S */
    ret |= es8388_write_reg(0x0C, 0x4C);
    /* ADC MCLK/采样率 = 256 */
    ret |= es8388_write_reg(0x0D, 0x02);
    /* ADC 数字音量 0dB */
    ret |= es8388_write_reg(0x10, 0x00);
    ret |= es8388_write_reg(0x11, 0x00);
    /* DAC 音频数据 16bit I2S */
    ret |= es8388_write_reg(0x17, 0x18);
    /* DAC MCLK/采样率 = 256 */
    ret |= es8388_write_reg(0x18, 0x02);
    /* DAC 数字音量 0dB (不衰减) */
    ret |= es8388_write_reg(0x1A, 0x00);
    ret |= es8388_write_reg(0x1B, 0x00);
    /* L/R 混频器 (DAC->通道2 通路使能) */
    ret |= es8388_write_reg(0x27, 0xB8);
    ret |= es8388_write_reg(0x2A, 0xB8);

    /* 播放配置: 使能 DAC 关 ADC, 双输出通道全开 */
    ret |= es8388_write_reg(0x02, 0x0A);     /* DAC 上电, ADC 关闭 */
    ret |= es8388_write_reg(0x04, 0x3C);     /* LOUT1/2 + ROUT1/2 全部使能 */
    /* 音量: 耳机口与板载喇叭均设 30/33 */
    ret |= es8388_write_reg(0x2E, 30);       /* HP VOL L */
    ret |= es8388_write_reg(0x2F, 30);       /* HP VOL R */
    ret |= es8388_write_reg(0x30, 30);       /* SPK VOL L */
    ret |= es8388_write_reg(0x31, 30);       /* SPK VOL R */
    /* 打开喇叭功放: SPK_EN 低电平有效 (官方音乐例程 xl9555_pin_write(SPK_EN_IO, 0)) */
    xl9555_pin_write(XL9555_SPK_EN, 0);
    ESP_LOGI(TAG, "SPK_EN pulled low (amp enabled)");

    vTaskDelay(pdMS_TO_TICKS(100));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "es8388 init failed");
        return ret;
    }
    /* 读回关键寄存器自检 */
    ESP_LOGI(TAG, "ES8388 check: R17(DAC fmt)=0x%02X R1A(DAC vol L)=0x%02X "
                  "R2E(HP vol L)=0x%02X R30(SPK vol L)=0x%02X R04(out)=0x%02X",
             es8388_read_reg(0x17), es8388_read_reg(0x1A),
             es8388_read_reg(0x2E), es8388_read_reg(0x30),
             es8388_read_reg(0x04));
    ESP_LOGI(TAG, "ES8388 init ok (addr 0x%02X)", ES8388_I2C_ADDR);
    return ESP_OK;
}
