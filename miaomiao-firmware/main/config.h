/**
 * 苗苗 ESP32 固件配置
 *
 * 硬件配置：
 * - ESP32-S3-DevKitC-1 (WROOM N16R8)
 * - INMP441 I2S 数字麦克风
 * - MAX98357A I2S 音频功放
 * - 1.54" TFT ST7789 SPI 显示屏
 *
 * 后端地址在 menuconfig 中配置，或修改下方默认值
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============== WiFi 配置 ==============
#define WIFI_SSID           "your-wifi-ssid"
#define WIFI_PASSWORD       "your-wifi-password"

// ============== 后端服务器 ==============
// 后端地址（不含路径）
#define BACKEND_HOST         "192.168.1.100"
#define BACKEND_PORT         8080

// API 端点
#define API_CHAT_SSE         "/api/audio"   // ESP32 音频对话
#define API_TTS              "/api/tts"     // TTS（预留）
#define API_HEALTH           "/api/health"  // 健康检查

// ============== 音频配置 ==============
// I2S 引脚配置 (ESP32-S3)
// 麦克风 INMP441: WS=GPIO1, SCK=GPIO2, SD=GPIO42
// 功放 MAX98357A: DIN=GPIO39, BCLK=GPIO40, LRC=GPIO41
#define I2S_WS_PIN           1      // Word Select (麦克风 WS, 功放 LRC)
#define I2S_SCK_PIN          2      // Serial Clock (麦克风 SCK, 功放 BCLK)
#define I2S_SD_PIN           42     // Serial Data 输入 (麦克风 SD)
#define I2S_DOUT_PIN         39     // Serial Data 输出 (功放 DIN)
#define I2S_AMP_BCLK_PIN     40     // 功放 BCLK（与麦克风 SCK 不同引脚）
#define I2S_AMP_LRC_PIN      41     // 功放 LRC（与麦克风 WS 不同引脚）
#define I2S_PORT             I2S_NUM_0

// 音频参数
#define AUDIO_SAMPLE_RATE    16000
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_CHANNELS        1     // 单声道 (MONO)

// ============== ST7789 TFT 显示屏配置 ==============
// ST7789 SPI 接口
#define OLED_SPI_HOST       SPI2_HOST
#define OLED_SCL_PIN        19     // CLK 时钟
#define OLED_SDA_PIN        20     // MOSI 数据
#define OLED_CS_PIN         45     // 片选
#define OLED_DC_PIN         47     // 数据/命令切换
#define OLED_RST_PIN        21     // 复位
#define OLED_BL_PIN         38     // 背光
#define OLED_WIDTH           240
#define OLED_HEIGHT          240

// ============== 功能开关 ==============
#define ENABLE_OTA           1     // 启用 OTA 更新
#define ENABLE_SCREEN        1      // 启用屏幕显示
#define ENABLE_TEST_BLINK   0      // 禁用测试闪烁，避免干扰

#endif // CONFIG_H
