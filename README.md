# ESP32-BOOM-Timer · C4 拆弹倒计时器

基于 **正点原子 ATK-DNESP32S3（老款）** 与 **ESP-IDF v5.4.4** 的 C4 拆弹倒计时器：

- 320×240 SPI-LCD（ST7789V）显示 **C4 背景图** + 时/分/秒大字倒计时
- **KEY0 一键切换 时/分/秒** 三个调整位，KEY1/KEY2 长按连续加减，KEY3 保存
- **BOOT 长按 3 秒 = 拆除炸弹** → 全屏"炸弹已拆除"大字 + 任务成功语音
- 倒计时结束 → **爆炸音效 + 任务失败语音** + 蜂鸣器报警
- 5 个嵌入音效（开机/开始计时/任务失败/任务成功/爆炸），软件衰减至 20% 音量
- 自定义分区表（App 分区 1.06 MB），34 汉字点阵字库，局部重绘去除屏闪

---

## 功能特性

1. **5 个按键**：KEY0 设置/切换调整位，KEY1 加 1，KEY2 减 1，KEY3 保存(设置中)/清除(主界面)，BOOT 开始/暂停/清除 + **长按 3 秒拆除**
2. **KEY0 三档切换**：设置界面按下 KEY0 依次切换 时 → 分 → 秒，KEY1/KEY2 按住连续加减
3. **SPI-LCD（ST7789V, 320×240）**：C4 背景图 + 倒计时大字 + 中文界面，设置界面局部重绘（无整屏闪烁）
4. **BOOT 长按 3 秒拆除**：倒计时运行中按住 BOOT 3 秒 → 播放"任务成功"语音、全屏显示"炸弹已拆除"（4 倍放大铺满 320 宽）
5. **倒计时结束**：先播爆炸音效再接"任务失败"语音，蜂鸣器持续报警 + LED 常亮
6. **5 个音效**（ES8388 + I2S）：开机语音 / 开始计时 C4 滴滴循环 / 任务失败 / 任务成功 / 爆炸，播放链路统一软件衰减至 **20%** 音量
7. **6 态状态机**：待机 → 设置 → 运行 → 暂停 → 报警 → 拆除成功
8. **串口命令终端**（UART0 115200）：help / mode / ui / set / start / pause / reset / status
9. **代码模块化**：components/ 组件分层（驱动/服务/应用四层），`tools/build.ps1` 一键构建

## 硬件接线（全部板载，无需外接）

依据正点原子《ESP32-S3使用指南—IDF版 V1.6》官方例程适配老款 ATK-DNESP32S3：

| 功能 | 引脚 | 说明 |
| ---- | ---- | ---- |
| KEY0（设置/切位） | XL9555 **P1_7** | 按下接 GND |
| KEY1（+1） | XL9555 **P1_6** | 按住连续加 |
| KEY2（-1） | XL9555 **P1_5** | 按住连续减 |
| KEY3（保存/清除） | XL9555 **P1_4** | 设置中保存，主界面清除计数 |
| BOOT（开始/暂停/拆除） | **GPIO0** | 短按控制，**运行中长按 3s = 拆除炸弹** |
| 蜂鸣器（板载，有源） | XL9555 **P0_3** | 低电平响 |
| 喇叭功放使能 SPK_EN | XL9555 **P0_2** | **低电平开启**（官方例程的坑，务必按低有效处理） |
| 板载 LED | **GPIO1** | 报警常亮（低电平点亮） |
| SPI-LCD（ST7789V） | SCLK=IO12, MOSI=IO11, MISO=IO13, CS=IO21, DC=IO40 | 插入底板 WIRELESS 接口 |
| LCD 复位/背光 | XL9555 **P1_2 / P1_3** | RST 低有效，背光高电平点亮 |
| IIC0（XL9555） | SCL=IO42, SDA=IO41 | 器件地址 0x20，400kHz |
| ES8388 音频 CODEC | I2S0: BCK=IO46, WS=IO9, DO=IO10, MCLK=IO3 | IIC 地址 0x10，走 IIC0 |
| UART0 TX/RX | 43 / 44 | 烧录/日志/命令共用 |

## 按键操作

| 界面 | KEY0 | KEY1 | KEY2 | KEY3 | BOOT |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 主界面（待机） | 进入设置 | - | - | 清除计数 | 开始计时 |
| 设置界面 | 切换 时/分/秒 | +1（长按连续） | -1（长按连续） | **保存**并返回 | 取消返回 |
| 运行中 | - | - | - | - | 暂停；**长按 3s = 拆除炸弹** |
| 已暂停 | - | - | - | - | 恢复计时 |
| 报警/拆除成功 | - | - | - | - | 停止报警/返回待机 |

> 上电/复位时按住 BOOT 会进入下载模式，属正常现象；运行中按下它就是普通按键。

## 状态机

```
IDLE(待机) --KEY0--> SETTING(设置) --KEY3--> IDLE
IDLE --BOOT--> RUNNING(运行) --BOOT--> PAUSED(暂停) --BOOT--> RUNNING
RUNNING --BOOT长按3s--> DEFUSED(拆除成功: 炸弹已拆除 + 任务成功语音)
RUNNING --归零--> ALARM(报警: 爆炸音效 + 任务失败语音 + 蜂鸣/LED)
ALARM/DEFUSED --BOOT--> IDLE
```

## 音效说明

| 时机 | 音效文件 | 说明 |
| ---- | ---- | ---- |
| 开始计时（按 BOOT） | snd_boot.wav | "炸弹已被安放"语音（3.2s，单次） |
| 计时中循环 | snd_start.wav | C4 下包滴滴声（6.9s，循环至结束） |
| 倒计时结束 | snd_explosion.wav | 爆炸音效（3s，先播） |
| 倒计时结束 | snd_over.wav | "任务失败"语音（2s，接爆炸后） |
| 拆除成功（长按 BOOT 3s） | snd_success.wav | "Mission Success"语音（3.5s） |

全部为 16kHz / 16bit / 单声道 WAV，经 `EMBED_FILES` 嵌入固件，播放链路统一 `s * 20 / 100` 软件衰减至 20% 音量。

## 串口命令（115200）

```
help                      # 帮助
mode countdown|stopwatch  # 切换模式（仅待机）
ui home|settings          # LCD 界面切换
set <HH:MM:SS>|<secs>     # 设置倒计时时间（仅待机，如 set 01:30 / set 90）
start                     # 开始 / 恢复
pause                     # 暂停
reset                     # 清零 / 停止报警
status                    # 查询模式与时间
```

## 目录结构

```
BOOM/
├── CMakeLists.txt            顶层构建
├── partitions.csv            自定义分区表（factory app 0x10000 / 0x110000 = 1.06MB）
├── sdkconfig                 构建配置（已启用自定义分区表）
├── sdkconfig.defaults        默认配置（esp32s3, -O2）
├── main/
│   ├── app_main.c            入口
│   └── CMakeLists.txt
├── components/
│   ├── common/               公共类型 + 引脚定义（pins.h, app_common.h 6态状态机）
│   ├── xl9555/               XL9555 IO 扩展芯片驱动（IIC0: 蜂鸣器/LCD/按键/功放）
│   ├── key_driver/           按键（KEY0-3 经 XL9555 轮询 + BOOT=GPIO0）
│   ├── lcd_driver/           SPI-LCD 驱动与绘图（ST7789V 320x240）
│   ├── lcd_font/             点阵字库（ASCII 8x16 + 中文 16x16, 34 汉字）
│   ├── buzzer_driver/        板载有源蜂鸣器（XL9555 P0_3）
│   ├── led_driver/           板载 LED（GPIO1）
│   ├── audio_player/         ES8388 + I2S 音效播放（5 音效, 后台任务+队列, 20% 音量）
│   ├── uart_cmd/             串口命令终端 + 日志输出
│   └── app_logic/            业务状态机 + LCD 界面（app_logic.c / app_ui.c + C4 背景图）
└── tools/
    ├── build.ps1             一键构建脚本（免手动 export）
    ├── gen_font.py           字库生成脚本（依赖 PIL，改 HZ_LIST 后运行）
    └── check_font.py         字库码点检查
```

## 编译与烧录

**方式一：一键脚本（推荐）**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\ESP32\Project\END\Boom\BOOM\tools\build.ps1
```

日志落 `%TEMP%\boom_buildN.log`，成功见 `Project build complete`，产物 `build\BOOM.bin`。

**方式二：IDF 命令行**

```powershell
. D:\ESP32\Espressif\frameworks\esp-idf-v5.4.4\export.ps1
cd D:\ESP32\Project\END\Boom\BOOM
idf.py build
idf.py -p COM5 flash monitor
```

> **⚠️ 重要：项目使用自定义分区表（`partitions.csv`），App 分区从 0x10000 起共 1.06MB。**
> 直接烧 `BOOM.bin` 会因分区不匹配失败，**必须全量烧录**（`idf.py -p COMx flash` 即可，脚本会自动写分区表）。
> 若曾烧过旧分区，建议先 `idf.py erase-flash` 再全量烧录。

> **本机环境备注**：若 `export.ps1` 报错找不到 `python_env\idf5.4_py3.13_env`，先执行：
> ```powershell
> $env:IDF_PYTHON_ENV_PATH='D:\ESP32\Espressif\python_env\idf5.4_py3.11_env'
> . D:\ESP32\Espressif\frameworks\esp-idf-v5.4.4\export.ps1
> idf.py build
> ```

## 素材与字库生成

- **背景图**：`components/app_logic/bg_image.bin`（320×240 RGB565 大端，153600B），由图片经 ffmpeg + Python 转制，已嵌入固件
- **音效**：源素材在 `D:\ESP32资料\`，统一转码为 16kHz/16bit/mono WAV 放入 `components/audio_player/sounds/`
- **字库**：新增汉字时修改 `tools/gen_font.py` 的 `HZ_LIST`，运行 `python tools/gen_font.py` 重新生成 `font_hz_16x16.c`，当前收录 34 字：
  `倒计时秒表设置暂停运行结束分清零开始模式时间状态已空闲止中炸弹已拆除`

## 免责声明

本项目为**娱乐学习用途**，界面与音效素材仅作仿真展示，请勿用于任何真实危险场景。
