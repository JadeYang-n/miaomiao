# 苗苗 ESP32 固件

基于 ESP32-S3 的语音交互设备固件，替代 PC 浏览器作为老人与苗苗对话的入口。

## 硬件配置

| 组件 | 型号 | 引脚 | 说明 |
|-----|------|------|------|
| 开发板 | ESP32-S3-DevKitC-1 (N16R8) | - | WROOM N16R8 模组 |
| 麦克风 | INMP441 | WS=1, SCK=2, SD=42 | I2S 数字麦克风 |
| 功放 | MAX98357A | DIN=39, BCLK=40, LRC=41 | I2S 音频功放 |
| 屏幕 | 1.54" TFT ST7789 | SCL=19, SDA=20, CS=45, DC=47, RST=21, BL=38 | SPI 240x240 |

### 屏幕引脚连接

| ST7789 引脚 | ESP32-S3 GPIO | 说明 |
|------------|----------------|------|
| SCL | GPIO 19 | SPI 时钟 |
| SDA | GPIO 20 | SPI 数据 |
| CS | GPIO 45 | 片选 |
| DC | GPIO 47 | 数据/命令切换 |
| RST | GPIO 21 | 复位 |
| BL | GPIO 38 | 背光控制 |
| VCC | 3.3V | 电源 |
| GND | GND | 地 |

## 功能

- [x] WiFi 连接管理（自动重连）
- [x] I2S 音频采集（INMP441）
- [x] I2S 音频播放（MAX98357A）
- [x] ST7789 TFT 显示（苗苗头像 + 对话文字）
- [x] HTTP 与后端通信
- [ ] 语音唤醒（VAD）- 待实现
- [ ] OTA 更新 - 待实现

## 目录结构

```
esp32-firmware/
├── CMakeLists.txt           # 项目构建配置
├── sdkconfig               # ESP-IDF 配置
├── main/
│   ├── CMakeLists.txt       # 组件构建配置
│   ├── config.h             # 硬件配置（WiFi、API 地址、屏幕等）
│   ├── main.c               # 主程序入口
│   ├── wifi.c / wifi.h      # WiFi 连接管理
│   ├── audio.c / audio.h    # I2S 音频采集/播放
│   ├── display.c / display.h # ST7789 TFT 显示驱动
│   └── http_client.c / http_client.h  # HTTP 客户端
├── build/                   # 编译输出目录
└── README.md
```

## 构建与烧录

### 环境要求

- ESP-IDF 5.4.4
- Python 3.11+
- ESP32-S3 工具链

### 编译步骤

由于 Windows 下 idf.py 有环境问题，使用手动 cmake 构建：

```powershell
# 设置环境
$env:IDF_PATH = 'C:\Users\vip\Espressif\frameworks\esp-idf-v5.4.4'
$env:ESP_ROM_ELF_DIR = 'C:\Users\vip\Espressif\tools\esp-rom-elfs\20241011'
$env:PATH = 'C:\Users\vip\Espressif\tools\cmake\3.30.2\bin;C:\Users\vip\Espressif\tools\ninja\1.12.1;C:\Users\vip\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Users\vip\Espressif\python_env\idf5.4_py3.11_env\Scripts;' + $env:PATH

cd esp32-firmware/build
cmake -G "Ninja" -DESP_PLATFORM=ON -DIDF_PATH=$env:IDF_PATH ..
ninja
```

### 烧录步骤

1. 擦除 flash：
```powershell
esptool.py --chip esp32s3 -p COM3 -b 460800 --before default_reset erase_flash
```

2. 烧录（一次性烧录所有分区）：
```powershell
esptool.py --chip esp32s3 -p COM3 -b 460800 --before default_reset write_flash `
    0x0 build/bootloader/bootloader.bin `
    0x8000 build/partition_table/partition-table.bin `
    0x10000 build/miaomiao-esp32.bin
```

3. 复位：
```powershell
esptool.py --chip esp32s3 -p COM3 -b 115200 --after hard_reset run
```

## 配置

`config.h` 中可配置：

```c
#define WIFI_SSID           "your-wifi-ssid"
#define WIFI_PASSWORD       "your-wifi-password"
#define BACKEND_HOST         "192.168.1.100"
#define BACKEND_PORT         8080
#define ENABLE_SCREEN       1      // 启用屏幕显示
#define ENABLE_TEST_BLINK   0     // 禁用测试闪烁
```

## 与后端通信

固件通过 HTTP 与后端通信：

```
POST /api/audio
Content-Type: application/json

{"audio":"base64编码的PCM音频","format":"pcm","sampleRate":16000}
```

后端返回：

```json
{"text":"苗苗的回复文字","audio":"base64编码的MP3音频"}
```

## 已知问题

1. **编译器 segfault**：ESP-IDF esp_lcd RGB 驱动在并行编译时会 segfault
   - 解决：使用单核编译或等待 IDF 修复

2. **屏幕显示问题**：屏幕闪烁几次后黑屏（2026-05-13）
   - 可能原因：显示内容为黑色背景，或背光控制问题
   - 状态：调查中

## 开发备忘

- Bootloader 偏移：0x0（ESP32-S3 默认）
- Partition Table 偏移：0x8000
- App 偏移：0x10000
- Flash 模式：DIO, 80MHz
- SPI 主机：SPI2_HOST