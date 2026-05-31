/**
 * 音频采集与播放模块
 *
 * 输入: INMP441 I2S 数字麦克风
 * 输出: MAX98357A I2S 音频功放
 * 注意：唤醒词检测已移除，使用 VAD 代替
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <driver/i2s_std.h>
#include <esp_log.h>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"
#include "model_path.h"
#include "config.h"

static const char *TAG = "AUDIO";

/* I2S 句柄 */
static i2s_chan_handle_t tx_handle;  // 播放（输出到 MAX98357A）
static i2s_chan_handle_t rx_handle;  // 录音（从 INMP441）

/* 音频缓存 */
static RingbufHandle_t audio_ringbuf = NULL;
static bool audio_initialized = false;

/* AFE 音频前端 */
static const esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
static int afe_feedsize = 0;
static int afe_fetchsize = 0;

/* 模型列表 */
static srmodel_list_t *models = NULL;

/**
 * 初始化 AFE 音频前端（包含 VAD + WakeNet）
 */
static esp_err_t init_afe(void)
{
    ESP_LOGI(TAG, "初始化 AFE 音频前端...");

    // 获取模型列表
    models = esp_srmodel_init("model");
    if (models == NULL) {
        ESP_LOGE(TAG, "获取模型列表失败");
        return ESP_FAIL;
    }

    // 初始化 AFE 配置 - 单麦克风输入格式 "M"
    afe_config_t *afe_cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (afe_cfg == NULL) {
        ESP_LOGE(TAG, "创建 AFE 配置失败");
        return ESP_FAIL;
    }

    // 配置 PCM
    afe_cfg->pcm_config.mic_num = 1;
    afe_cfg->pcm_config.sample_rate = AUDIO_SAMPLE_RATE;

    // 关键修复：使用 PSRAM 内存分配，避免内部 RAM 耗尽
    afe_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    // 减小 ring buffer 大小以节省内存
    afe_cfg->afe_ringbuf_size = 3;

    // 禁用 VAD 以节省内存（用于唤醒词检测）
    afe_cfg->vad_init = false;

    // 禁用 AGC
    afe_cfg->agc_init = false;

    // 禁用 WakeNet，使用独立的唤醒词检测模块
    afe_cfg->wakenet_init = false;
    afe_cfg->wakenet_model_name = NULL;

    // 打印配置
    afe_config_print(afe_cfg);

    // 创建 AFE 实例
    afe_handle = esp_afe_handle_from_config(afe_cfg);
    afe_data = afe_handle->create_from_config(afe_cfg);
    afe_config_free(afe_cfg);

    // 在 afe_config_free 之后再次检查 AFE 是否创建成功
    // 注意：即使 SR_RINGBUF 报错，AFE 仍可能部分初始化
    if (afe_data == NULL) {
        ESP_LOGE(TAG, "创建 AFE 实例失败");
        return ESP_FAIL;
    }

    afe_feedsize = afe_handle->get_feed_chunksize(afe_data);
    afe_fetchsize = afe_handle->get_fetch_chunksize(afe_data);

    ESP_LOGI(TAG, "AFE 初始化完成");
    ESP_LOGI(TAG, "  feed size: %d samples", afe_feedsize);
    ESP_LOGI(TAG, "  fetch size: %d samples", afe_fetchsize);

    return ESP_OK;
}

/**
 * 初始化 I2S 音频硬件
 */
esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "初始化音频硬件...");

    /* 配置 I2S 通道（输入输出） */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(AUDIO_BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = -1,
            .bclk = I2S_SCK_PIN,     /* 共享 BCLK：麦克风 SCK 和 功放 BCLK */
            .ws = I2S_WS_PIN,        /* WS/LRC 时钟 */
            .dout = I2S_DOUT_PIN,     /* 数据输出：功放 DIN */
            .din = I2S_SD_PIN,        /* 数据输入：麦克风 SD */
            .invert_flags = {
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    /* 配置单 slot（MONO） */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));

    /* 初始化 AFE 音频前端 */
    if (init_afe() != ESP_OK) {
        ESP_LOGE(TAG, "AFE 初始化失败");
        return ESP_FAIL;
    }

    /* 注意：暂时禁用 WakeNet 检测（内存不足）
     * 唤醒词功能需要后续优化
     * 当前使用 VAD 检测代替唤醒词触发对话
     */
#if 0
    /* 初始化独立唤醒词检测 */
    ESP_LOGI(TAG, "初始化独立唤醒词检测...");
    if (wake_word_init() != ESP_OK) {
        ESP_LOGW(TAG, "唤醒词检测初始化失败，将无法检测唤醒词");
    }
#endif

    /* 创建音频环形缓冲区 */
    audio_ringbuf = xRingbufferCreate(8192, RINGBUF_TYPE_NOSPLIT);
    if (audio_ringbuf == NULL) {
        ESP_LOGE(TAG, "创建音频缓冲区失败");
        return ESP_FAIL;
    }

    audio_initialized = true;
    ESP_LOGI(TAG, "音频硬件初始化完成");
    return ESP_OK;
}

/**
 * 检查是否检测到唤醒词（已禁用，返回 false）
 */
bool audio_is_wake_word_detected(void)
{
    return false;
}

/**
 * 重置唤醒词检测状态（已禁用）
 */
void audio_reset_wake_word_detection(void)
{
    // 不做任何事
}

/**
 * 采集音频（阻塞直到采集完成或超时）
 * @param buffer 输出参数，指向接收音频数据的 buffer（调用者负责释放）
 * @param max_samples 最大采样点数
 * @return 实际采样点数，<= 0 表示超时或错误
 */
int audio_capture(int16_t **buffer, int max_samples)
{
    if (!audio_initialized) {
        return -1;
    }

    size_t bytes_read = 0;
    int16_t *samples = (int16_t *)malloc(max_samples * sizeof(int16_t));
    if (samples == NULL) {
        return -1;
    }

    /* 启动录音 */
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    /* 读取音频数据（阻塞最多 3 秒） */
    esp_err_t ret = i2s_channel_read(rx_handle, samples, max_samples * sizeof(int16_t), &bytes_read, 3000);

    /* 停止录音以节省资源 */
    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));

    if (ret != ESP_OK || bytes_read == 0) {
        free(samples);
        return 0;  // 超时或无数据
    }

    *buffer = samples;
    return bytes_read / sizeof(int16_t);
}

/**
 * 使用独立唤醒词检测采集音频
 * @param buffer 输出参数，指向接收音频数据的 buffer（调用者负责释放）
 * @param max_samples 最大采样点数
 * @return 实际采样点数，<= 0 表示超时或错误
 */
int audio_capture_with_wakeword(int16_t **buffer, int max_samples)
{
    // 直接调用 audio_capture，忽略唤醒词检测
    return audio_capture(buffer, max_samples);
}

/**
 * 播放音频
 * @param data 音频数据（PCM 16bit mono）
 * @param len 数据长度（字节）
 */
void audio_play(uint8_t *data, size_t len)
{
    if (!audio_initialized || tx_handle == NULL) {
        return;
    }

    /* 启动播放 */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, data, len, &bytes_written, 1000);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "音频播放失败: %d", ret);
    }

    /* 等待播放完成 */
    vTaskDelay(500 / portTICK_PERIOD_MS);

    /* 停止播放 */
    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
}

/**
 * 播放提示音（简单的 beep）
 */
void audio_play_beep(void)
{
    /* 生成 440Hz 正弦波 beep，100ms */
    const int sample_rate = AUDIO_SAMPLE_RATE;
    const int duration_ms = 100;
    const int num_samples = sample_rate * duration_ms / 1000;
    int16_t *beep = (int16_t *)malloc(num_samples * sizeof(int16_t));

    if (beep == NULL) return;

    for (int i = 0; i < num_samples; i++) {
        beep[i] = (int16_t)(32767 * 0.5 * sin(2 * 3.14159 * 440 * i / sample_rate));
    }

    audio_play((uint8_t *)beep, num_samples * sizeof(int16_t));
    free(beep);
}