# BOOM 简易倒计时计时器 (项目6) —— 正点原子 ATK-DNESP32S3

ESP32-S3 实现的「简易倒计时计时器」, 满足需求清单全部 8 项:

1. **5 个按键**: KEY0 进入设置 / KEY1 时间+1 / KEY2 时间-1 / KEY3 保存或清除计数 / BOOT 开始暂停停止
2. **SPI-LCD**: 正点原子 SPI LCD 模块 (ST7789V, 320x240) 中文显示倒计时数字 + 时钟计时图标
3. **倒计时结束**: LED 常亮 + 蜂鸣器持续响, 按 BOOT 或 KEY3 清零停止
4. **串口终端控制**: UART0 (115200) 发送指令控制各模块, 并可切换 LCD 界面
5. **代码模块化**: components/ 组件分层 (含 XL9555 扩展芯片、ES8388 音频 CODEC)
6. **UART 输出计时启停记录**: 串口打印 `[LOG] ...`
7. **最后 3 秒预警**: 蜂鸣器间断"滴滴"
8. **秒表(正计时)模式**: 待机时长按 BOOT 切换倒计时/秒表
9. **三个音效**: 开机 / 开始计时 / 计时结束 分别播放板载喇叭音效

## 硬件接线 (全部板载, 无需外接)

本工程按 ATK-DNESP32S3 老款开发板板载资源适配,
依据正点原子《ESP32-S3使用指南—IDF版 V1.6》官方例程:

| 功能 | 引脚 | 说明 |
| ---- | ---- | ---- |
| KEY1 = 板载 KEY0 | XL9555 **P1_7** | 进入设置时间 (按下接 GND) |
| KEY2 = 板载 KEY1 | XL9555 **P1_6** | 时间 +1 |
| KEY3 = 板载 KEY2 | XL9555 **P1_5** | 时间 -1 |
| KEY4 = 板载 KEY3 | XL9555 **P1_4** | 保存 / 清除计数 |
| KEY5 = BOOT 按键 | **GPIO0** | 开始/暂停/停止; 待机时长按切模式 |
| 蜂鸣器 (板载, 有源) | XL9555 **P0_3** | 低电平响, 由扩展芯片控制 |
| 板载 LED | **GPIO1** | 倒计时结束常亮 (低电平点亮) |
| SPI-LCD 模块 (ST7789V) | SCLK=IO12, MOSI=IO11, MISO=IO13 | 插入底板 WIRELESS 接口 |
| LCD CS / DC | **IO21 / IO40** | - |
| LCD 复位 / 背光 | XL9555 **P1_2 / P1_3** | RST 低有效, 背光高电平点亮 |
| IIC0 (XL9555) | SCL=IO42, SDA=IO41 | 器件地址 0x20, 400kHz |
| ES8388 音频 CODEC | I2S: BCK=IO46, WS=IO9, DO=IO10, MCLK=IO3 | IIC 地址 0x10, 走 IIC0; 板载喇叭/耳机输出 |
| UART0 TX/RX | 43 / 44 | 烧录/日志/命令共用 |

> 屏幕说明: 工程默认 2.4 寸模块 (320x240 横屏)。
> 若你的是 1.3 寸 ATK-MD0130 (240x240), 修改
> `components/lcd_driver/lcd_driver.h` 的 `LCD_WIDTH/LCD_HEIGHT` 为 240,
> 并把 `lcd_driver.c` 初始化序列中 MADCTL 改为竖屏参数即可 (或联系我改)。

## 按键操作 (按板载丝印)

| 界面 | KEY0 | KEY1 | KEY2 | KEY3 | BOOT |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 主界面 | 进入设置时间 | - | - | 清除计数 | 开始 / 暂停 |
| 设置界面 | - | 时间 +1 秒 (按住连续) | 时间 -1 秒 (按住连续) | 保存并返回 | 取消返回 |
| 报警(已结束) | - | - | - | - | 停止报警并清零 |
| 待机(主界面) | - | - | - | - | **长按 1s**: 切换倒计时/秒表 |

> 设置界面不再分 时/分/秒 档位: KEY1/KEY2 每次只调 1 秒并自动进位,
> 按住 KEY1/KEY2 会连续加减, 设长时间不再费劲。

> 上电/复位时按住 BOOT 会进入下载模式, 属正常现象;
> 运行中按下它就是普通按键 (开始/暂停/停止)。

## 串口命令 (115200)

```
help                      # 帮助
mode countdown|stopwatch  # 切换模式 (仅待机)
ui home|settings          # LCD 界面切换
set <HH:MM:SS>|<secs>     # 设置倒计时时间 (仅待机, 如 set 01:30 / set 90)
start                     # 开始 / 恢复
pause                     # 暂停
reset                     # 清零 / 停止报警
status                    # 查询模式与时间
```

UART 计时启停记录示例:

```
[LOG] countdown START countdown @123s
[LOG] countdown PAUSED remain=00:00:45 @131s
[LOG] countdown ALARM time up @213s
[LOG] stopwatch RESET stopwatch cleared @402s
```

## 音效说明

三个 WAV 音效嵌入固件, 由板载 ES8388 + 喇叭播放 (16kHz/16bit/单声道):

| 时机 | 音效 | 说明 |
| ---- | ---- | ---- |
| 开机 | snd_boot.wav | 上电初始化完成后播放 (开局音效) |
| 开始计时 | snd_start.wav | 待机首次按下 BOOT 开始计时时播放 |
| 计时结束 | snd_over.wav | 倒计时归零进入报警时播放 (爆炸音效) |

> 源音频在 `D:\ESP32资料\` 下 (01_开局 / 02_下包后_C4滴滴滴 / 03_倒计时结束_爆炸,
> .mp3), 已用 ffmpeg 转码为 WAV 放入 `components/audio_player/sounds/`。
> 播放为后台任务, 正在播放时新请求忽略; 音效播放不影响蜂鸣器预警/报警。

## 目录结构

```
BOOM/
├── CMakeLists.txt            顶层构建
├── sdkconfig.defaults        默认配置 (esp32s3, -O2)
├── main/
│   ├── app_main.c            入口
│   └── CMakeLists.txt
├── components/
│   ├── common/               引脚与公共类型 (板载引脚见 pins.h)
│   ├── xl9555/               XL9555 IO 扩展芯片驱动 (IIC0, 蜂鸣器/LCD/按键)
│   ├── key_driver/           按键 (KEY0-3 经 XL9555 轮询 + KEY5=BOOT)
│   ├── lcd_driver/           SPI-LCD 驱动与绘图 (ST7789V 320x240)
│   ├── lcd_font/             点阵字库 (ASCII 8x16 + 中文 16x16)
│   ├── buzzer_driver/        板载有源蜂鸣器 (XL9555 P0_3, 滴滴预警/持续报警)
│   ├── led_driver/           板载 LED (GPIO1)
│   ├── audio_player/         板载 ES8388 CODEC + I2S 音效播放 (开机/开始/结束)
│   ├── uart_cmd/             串口命令终端 + 日志输出
│   └── app_logic/            业务状态机 + LCD 界面 (app_logic.c / app_ui.c)
└── tools/
    └── gen_font.py           字库生成脚本 (依赖 PIL)
```

## 编译与烧录

```powershell
# 打开 ESP-IDF v5.4.4 环境
. D:\ESP32\Espressif\frameworks\esp-idf-v5.4.4\export.ps1

cd D:\ESP32\Project\END\Boom\BOOM
idf.py set-target esp32s3     # 首次需要
idf.py build
idf.py -p COM5 flash monitor
```

> **本机环境备注**: 本机 ESP-IDF 的 Python 虚拟环境实际为
> `idf5.4_py3.11_env`, 若 `export.ps1` 报错找不到
> `python_env\idf5.4_py3.13_env`, 先执行:
> ```powershell
> $env:IDF_PYTHON_ENV_PATH='D:\ESP32\Espressif\python_env\idf5.4_py3.11_env'
> . D:\ESP32\Espressif\frameworks\esp-idf-v5.4.4\export.ps1
> idf.py build
> ```

> 字库由 `tools/gen_font.py` 预生成; 若界面新增汉字, 修改脚本中的
> `HZ_LIST` 后重新运行 `python tools/gen_font.py` 即可。
> 本工程已在 ESP-IDF v5.4.4 下编译通过 (见 `build/`)。
