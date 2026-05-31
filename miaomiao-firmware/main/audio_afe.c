/**
 * AFE 音频前端实现
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <model_path.h>

#include "audio_afe.h"
#include "wake_word.h"
#include "config.h"

static const char *TAG = "AUDIO_AFE";

// 全局 AFE 实例（用于 audio_level 查询）
audio_afe_t* g_afe_instance = NULL;

struct audio_afe {
    esp_afe_sr_iface_t* afe_iface;
    esp_afe_sr_data_t* afe_data;

    i2s_chan_handle_t rx_handle;
    i2s_chan_handle_t tx_handle;  // TX 通道（与 RX 共享 I2S 总线）

    audio_afe_config_t config;

    TaskHandle_t feed_task_handle;
    TaskHandle_t fetch_task_handle;
    bool running;

    int16_t* feed_buffer;
    int feed_buffer_size;

    // AFE 增强后音频输出回调
    afe_output_callback_t output_callback;

    // 最近一帧音频能量（用于能量检测）
    volatile int last_audio_level;
};

static void feed_task(void* arg)
{
    audio_afe_t* afe = (audio_afe_t*)arg;
    int feed_samples = afe->afe_iface->get_feed_chunksize(afe->afe_data);

    // CRITICAL: Log immediately to prove task is running
    ESP_LOGI(TAG, "##############################################");
    ESP_LOGI(TAG, "FEED TASK STARTED core=%d samples=%d run=%d",
             xPortGetCoreID(), feed_samples, (int)afe->running);
    ESP_LOGI(TAG, "##############################################");

    // Use int32_t buffer for 32-bit I2S data (xiaozhi approach)
    int32_t* raw_buffer = (int32_t*)malloc(feed_samples * sizeof(int32_t));
    int16_t* conv_buffer = (int16_t*)malloc(feed_samples * sizeof(int16_t));
    if (raw_buffer == NULL || conv_buffer == NULL) {
        ESP_LOGE(TAG, "分配缓冲区失败");
        if (raw_buffer) free(raw_buffer);
        if (conv_buffer) free(conv_buffer);
        vTaskDelete(NULL);
        return;
    }

    while (afe->running) {
        size_t bytes_read = 0;
        // Read 32-bit samples from I2S
        esp_err_t ret = i2s_channel_read(afe->rx_handle, raw_buffer,
                                         feed_samples * sizeof(int32_t),
                                         &bytes_read, 100);

        if (ret == ESP_OK && bytes_read > 0) {
            int samples_read = bytes_read / sizeof(int32_t);

            // 32-bit → 16-bit（xiaozhi 方式：右移 12 位 + clamp）
            for (int i = 0; i < samples_read; i++) {
                int32_t value = raw_buffer[i] >> 12;
                conv_buffer[i] = (value > INT16_MAX) ? INT16_MAX :
                                 (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value;
            }

            // Feed to VAD AFE
            afe->afe_iface->feed(afe->afe_data, conv_buffer);

            // Log wake word result every 32 iterations
            static int wn_log_counter = 0;
            if (wn_log_counter++ % 32 == 0) {
                // Find max sample in this chunk for diagnostics
                int16_t max_sample = 0;
                for (int i = 0; i < samples_read; i++) {
                    int16_t abs_val = conv_buffer[i] > 0 ? conv_buffer[i] : -conv_buffer[i];
                    if (abs_val > max_sample) max_sample = abs_val;
                }
                ESP_LOGI(TAG, "AFE FEED: max_audio=%d", max_sample);
            }

            // Debug: log raw audio level every 100 iterations
            static int raw_debug_counter = 0;
            if (raw_debug_counter++ % 100 == 0) {
                int16_t max_sample = 0;
                int32_t sum_sample = 0;
                for (int i = 0; i < samples_read && i < 512; i++) {
                    int16_t abs_val = conv_buffer[i] > 0 ? conv_buffer[i] : -conv_buffer[i];
                    if (abs_val > max_sample) max_sample = abs_val;
                    sum_sample += abs_val;
                }
                int avg_sample = sum_sample / 512;
                ESP_LOGI(TAG, "RAW AUDIO: max=%d, avg=%d, samples=%d", max_sample, avg_sample, samples_read);
                afe->last_audio_level = avg_sample;
            }

            // Log feed loop activity every 32 iterations
            static int loop_counter = 0;
            if (loop_counter++ % 32 == 0) {
                ESP_LOGI(TAG, "FEED LOOP: running=%d", (int)afe->running);
            }
        }
    }

    free(raw_buffer);
    free(conv_buffer);
    vTaskDelete(NULL);
}

static void fetch_task(void* arg)
{
    audio_afe_t* afe = (audio_afe_t*)arg;
    int fetch_samples = afe->afe_iface->get_fetch_chunksize(afe->afe_data);

    ESP_LOGI(TAG, "AFE fetch 任务启动: fetch_samples=%d", fetch_samples);

    while (afe->running) {
        // Use fetch_with_delay like xiaozhi does
        afe_fetch_result_t* result = afe->afe_iface->fetch_with_delay(afe->afe_data, portMAX_DELAY);

        if (result != NULL && result->ret_value != ESP_FAIL) {
            // 调试：每次 fetch 都打印 wake_word（限50次）
            static int dbg_counter = 0;
            if (dbg_counter < 50) {
                ESP_LOGI(TAG, "WakeWord DBG: wake_word=%d, wn_idx=%d, state=%d, vad=%d",
                         result->wake_word_index, result->wakenet_model_index, result->wakeup_state, result->vad_state);
                dbg_counter++;
            }
            // Log when state, wake_word, or wn_idx changes from 0
            static int debug_counter = 0;
            if (debug_counter++ % 50 == 0) {
                ESP_LOGI(TAG, "WakeWord DBG: wake_word=%d, wn_idx=%d, state=%d, vad=%d, vol=%.1fdB",
                         result->wake_word_index, result->wakenet_model_index, result->wakeup_state, result->vad_state,
                         result->data_volume);
            }
            // Also log raw wakeup_state from VAD AFE every 5s
            static int64_t last_vad_log_ms = 0;
            int64_t now_ms2 = esp_timer_get_time() / 1000;
            if (now_ms2 - last_vad_log_ms > 5000) {
                ESP_LOGI(TAG, "[VAD AFE RAW] wakeup_state=%d (0x%X), wake_word_index=%d, wakenet_model_index=%d",
                         result->wakeup_state, result->wakeup_state, result->wake_word_index, result->wakenet_model_index);
                last_vad_log_ms = now_ms2;
            }
            // 调试：每 50 次打印一次完整状态
            // Also trigger when wake_word=1 is detected (even if state not fully confirmed)
            // This is needed because with VAD enabled, state may not reach WAKENET_DETECTED
            // Only trigger if wake_word_index transitions from 0 to 1 (not repeatedly while already 1)
            static int last_wake = 0;
            if (result->wake_word_index == 1 && last_wake == 0) {
                last_wake = 1;
                ESP_LOGI(TAG, "##### 唤醒词检测到 (wake_word=1)! wn_model_idx=%d, state=%d #####",
                         result->wakenet_model_index, result->wakeup_state);
                ESP_LOGI(TAG, ">>> VAD AFE on_wake_word callback = %p", afe->config.on_wake_word);
                if (afe->config.on_wake_word) {
                    afe->config.on_wake_word("miaomiao", result->wake_word_index);
                }
            } else if (result->wake_word_index == 0) {
                last_wake = 0;
            }

            // Primary approach: check wakeup_state (like xiaozhi does)
            if (result->wakeup_state == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "##### 唤醒词检测到 (wakeup_state)! wake_word=%d, wn_idx=%d #####",
                         result->wake_word_index, result->wakenet_model_index);
                if (afe->config.on_wake_word) {
                    afe->config.on_wake_word("miaomiao", result->wake_word_index);
                }
            }

            if (result->vad_state == VAD_SPEECH) {
                ESP_LOGD(TAG, "VAD: SPEECH");
                if (afe->config.on_vad_change) {
                    afe->config.on_vad_change(true);
                }
            } else if (result->vad_state == VAD_SILENCE) {
                ESP_LOGD(TAG, "VAD: SILENCE");
                if (afe->config.on_vad_change) {
                    afe->config.on_vad_change(false);
                }
            }

            // 调用输出回调（可用于录音）
            static int dbg_fetch_count = 0;
            if (afe->output_callback) {
                if (result->data && result->data_size > 0) {
                    int samples = result->data_size / sizeof(int16_t);
                    if (dbg_fetch_count < 10) {
                        ESP_LOGI(TAG, ">>> FETCH_CB: data=%p, size=%d, samples=%d, callback=%p",
                                 result->data, result->data_size, samples, afe->output_callback);
                        dbg_fetch_count++;
                    }
                    afe->output_callback(result->data, samples);
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

static esp_err_t init_i2s(audio_afe_t* afe)
{
    /*
     * xiaozhi NoAudioCodecSimplex 方案：
     * TX (功放) 和 RX (麦克风) 使用不同的 I2S 端口，完全独立。
     * - TX: I2S_NUM_0, BCLK=GPIO40, WS=GPIO41, DOUT=GPIO39
     * - RX: I2S_NUM_1, BCLK=GPIO2,  WS=GPIO1,  DIN=GPIO42
     */

    /* === 创建 TX 通道 (I2S_NUM_0) === */
    i2s_chan_config_t tx_chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    esp_err_t ret = i2s_new_channel(&tx_chan_cfg, &afe->tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 TX 通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 24000,  /* MiMo TTS 返回 24kHz，直接匹配 */
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_AMP_BCLK_PIN,
            .ws = I2S_AMP_LRC_PIN,
            .dout = I2S_DOUT_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_LOGI(TAG, "TX: port=0, bclk=%d, ws=%d, dout=%d",
             I2S_AMP_BCLK_PIN, I2S_AMP_LRC_PIN, I2S_DOUT_PIN);

    ret = i2s_channel_init_std_mode(afe->tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S TX 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* === 创建 RX 通道 (I2S_NUM_1) === */
    i2s_chan_config_t rx_chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ret = i2s_new_channel(&rx_chan_cfg, NULL, &afe->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 RX 通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    std_cfg.clk_cfg.sample_rate_hz = AUDIO_SAMPLE_RATE;  /* RX 保持 16kHz 给 WakeNet/VAD */
    std_cfg.gpio_cfg.bclk = I2S_SCK_PIN;
    std_cfg.gpio_cfg.ws = I2S_WS_PIN;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = I2S_SD_PIN;

    ESP_LOGI(TAG, "RX: port=1, bclk=%d, ws=%d, din=%d",
             I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN);

    ret = i2s_channel_init_std_mode(afe->rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S RX 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(afe->rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 I2S RX 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* TX 启用后保持常开（xiaozhi 方式：创建后直接 enable，播放时只写数据） */
    ret = i2s_channel_enable(afe->tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 I2S TX 失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2S 初始化完成（Simplex: TX=port0 常开, RX=port1）");
    return ESP_OK;
}

audio_afe_t* audio_afe_create(audio_afe_config_t* config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "配置为空");
        return NULL;
    }

    audio_afe_t* afe = (audio_afe_t*)calloc(1, sizeof(audio_afe_t));
    if (afe == NULL) {
        ESP_LOGE(TAG, "分配 AFE 结构失败");
        return NULL;
    }

    memcpy(&afe->config, config, sizeof(audio_afe_config_t));

    // 初始化模型列表
    srmodel_list_t* models = esp_srmodel_init("model");
    ESP_LOGI(TAG, "esp_srmodel_init 返回: models=%p, num=%d", models, models ? models->num : -999);
    if (models == NULL || models->num <= 0) {
        ESP_LOGE(TAG, "模型初始化失败: models=%p, num=%d", models, models ? models->num : -999);
        if (models && models->num == -1) {
            ESP_LOGE(TAG, "num=-1 表示模型数据未正确加载");
        }
        free(afe);
        return NULL;
    }

    ESP_LOGI(TAG, "开始查找 wakenet 模型，共 %d 个模型", models->num);

    // 查找 wakenet 模型 - 精确匹配 wn9 + himiaomiao
    char* wakenet_model = NULL;
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "模型 %d: %s", i, models->model_name[i]);
        if (strstr(models->model_name[i], "wn9") != NULL &&
            strstr(models->model_name[i], "himiaomiao") != NULL) {
            wakenet_model = models->model_name[i];
            ESP_LOGI(TAG, "找到 wakenet 模型: %s", wakenet_model);
            break;
        }
    }

    // 如果没找到精确匹配，退而求其次找 wn9
    if (wakenet_model == NULL) {
        for (int i = 0; i < models->num; i++) {
            if (strstr(models->model_name[i], "wn9") != NULL) {
                wakenet_model = models->model_name[i];
                ESP_LOGI(TAG, "退而求其次找到 wakenet 模型: %s", wakenet_model);
                break;
            }
        }
    }

    if (wakenet_model == NULL) {
        ESP_LOGE(TAG, "未找到 wakenet 模型");
        free(afe);
        return NULL;
    }

    // xiaozhi uses HIGH_PERF mode for better wake word detection
    afe_config_t* afe_cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (afe_cfg == NULL) {
        ESP_LOGE(TAG, "AFE 配置初始化失败，尝试 HIGH_PERF...");
        afe_cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
        if (afe_cfg == NULL) {
            ESP_LOGE(TAG, "AFE 配置初始化失败");
            free(afe);
            return NULL;
        }
    }

    // 关闭 AEC（无回声消除）
    afe_cfg->aec_init = false;

    // 根据配置决定是否启用 WakeNet
    if (config->wake_word_model != NULL) {
        afe_cfg->wakenet_init = true;
        afe_cfg->wakenet_model_name = (char*)config->wake_word_model;
        ESP_LOGI(TAG, "启用 WakeNet: model=%s", config->wake_word_model);
    } else {
        afe_cfg->wakenet_init = false;
    }

    // 启用 VAD - VAD_MODE_1 标准灵敏度
    if (config->vad_init) {
        afe_cfg->vad_init = true;
        afe_cfg->vad_mode = VAD_MODE_1;  // VAD_MODE_1 (标准灵敏度)
        if (config->vad_model != NULL) {
            afe_cfg->vad_model_name = (char*)config->vad_model;
        }
    }

    // 降噪
    if (config->ns_init) {
        afe_cfg->ns_init = true;
        afe_cfg->afe_ns_mode = AFE_NS_MODE_NET;
        if (config->ns_model != NULL) {
            afe_cfg->ns_model_name = (char*)config->ns_model;
        }
    }

    // 内存分配
    afe_cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    // CPU affinity and priority - like xiaozhi uses
    afe_cfg->afe_perferred_core = 1;
    afe_cfg->afe_perferred_priority = 1;

    // PCM 配置
    afe_cfg->pcm_config.total_ch_num = 1;
    afe_cfg->pcm_config.mic_num = 1;
    afe_cfg->pcm_config.sample_rate = config->sample_rate > 0 ? config->sample_rate : AUDIO_SAMPLE_RATE;

    // 增益：2.5 太高会导致噪音被放大，VAD 持续报 speech
    afe_cfg->afe_linear_gain = 1.0f;

    // Lower threshold for WakeNet to be more sensitive (0.35 instead of 0.42)
    // Note: set_wakenet_threshold is called AFTER AFE creation (line ~370)

    ESP_LOGI(TAG, "创建 AFE...");
    ESP_LOGI(TAG, "  - mode: %s", afe_cfg->afe_mode == AFE_MODE_HIGH_PERF ? "HIGH_PERF" : "LOW_COST");
    ESP_LOGI(TAG, "  - wakenet_init: %d, model: '%s'", afe_cfg->wakenet_init, afe_cfg->wakenet_model_name ? afe_cfg->wakenet_model_name : "NULL");
    ESP_LOGI(TAG, "  - vad_init: %d", afe_cfg->vad_init);
    ESP_LOGI(TAG, "  - ns_init: %d", afe_cfg->ns_init);
    ESP_LOGI(TAG, "  - sample_rate: %d", afe_cfg->pcm_config.sample_rate);
    ESP_LOGI(TAG, "  - afe_linear_gain: %.1f", afe_cfg->afe_linear_gain);

    // 检查配置
    afe_config_check(afe_cfg);

    const esp_afe_sr_iface_t* iface = esp_afe_handle_from_config(afe_cfg);
    if (iface == NULL) {
        ESP_LOGE(TAG, "AFE 接口获取失败");
        afe_config_free(afe_cfg);
        free(afe);
        return NULL;
    }

    afe->afe_iface = (esp_afe_sr_iface_t*)iface;
    afe->afe_data = iface->create_from_config(afe_cfg);
    if (afe->afe_data == NULL) {
        ESP_LOGE(TAG, "AFE 创建失败");
        afe_config_free(afe_cfg);
        free(afe);
        return NULL;
    }

    if (init_i2s(afe) != ESP_OK) {
        afe_config_free(afe_cfg);
        free(afe);
        return NULL;
    }

    // Now safe to free config
    afe_config_free(afe_cfg);

    ESP_LOGI(TAG, "AFE 创建成功");

    // 保存全局实例
    g_afe_instance = afe;

    // 设置 WakeNet 阈值（必须在 afe_config_free 之前调用）
    // Range 0.4-0.9999, lower = more sensitive
    int ret_threshold = afe->afe_iface->set_wakenet_threshold(afe->afe_data, 1, 0.35f);
    ESP_LOGI(TAG, "set_wakenet_threshold(0.35) returned: %d", ret_threshold);

    return afe;
}

void audio_afe_destroy(audio_afe_t* afe)
{
    if (afe == NULL) return;

    audio_afe_stop(afe);

    if (afe->afe_iface != NULL && afe->afe_data != NULL) {
        afe->afe_iface->destroy(afe->afe_data);
        afe->afe_data = NULL;
        afe->afe_iface = NULL;
    }

    if (afe->tx_handle != NULL) {
        i2s_channel_disable(afe->tx_handle);
        i2s_del_channel(afe->tx_handle);
        afe->tx_handle = NULL;
    }

    if (afe->rx_handle != NULL) {
        i2s_channel_disable(afe->rx_handle);
        i2s_del_channel(afe->rx_handle);
        afe->rx_handle = NULL;
    }

    free(afe);
}

int audio_afe_start(audio_afe_t* afe)
{
    if (afe == NULL) return -1;

    if (afe->running) {
        ESP_LOGW(TAG, "AFE 已经在运行");
        return 0;
    }

    afe->running = true;

    // 创建 feed 任务 - Core 0
    BaseType_t ret = xTaskCreatePinnedToCore(
        feed_task,
        "afe_feed",
        4096,
        afe,
        5,
        &afe->feed_task_handle,
        0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 AFE feed 任务失败");
        afe->running = false;
        return -1;
    }

    // 创建 fetch 任务 - Core 1
    ret = xTaskCreatePinnedToCore(
        fetch_task,
        "afe_fetch",
        4096,
        afe,
        5,
        &afe->fetch_task_handle,
        1
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 AFE fetch 任务失败");
        afe->running = false;
        return -1;
    }

    ESP_LOGI(TAG, "AFE 已启动");

    // Wait for AFE tasks to start
    vTaskDelay(100 / portTICK_PERIOD_MS);

    return 0;
}

void audio_afe_stop(audio_afe_t* afe)
{
    if (afe == NULL) return;

    if (!afe->running) return;

    afe->running = false;

    if (afe->feed_task_handle != NULL) {
        vTaskDelete(afe->feed_task_handle);
        afe->feed_task_handle = NULL;
    }

    if (afe->fetch_task_handle != NULL) {
        vTaskDelete(afe->fetch_task_handle);
        afe->fetch_task_handle = NULL;
    }

    ESP_LOGI(TAG, "AFE 已停止");
}

void audio_afe_feed(audio_afe_t* afe, const int16_t* data, int samples)
{
    (void)afe;
    (void)data;
    (void)samples;
}

int audio_afe_get_feed_samples(audio_afe_t* afe)
{
    if (afe == NULL || afe->afe_iface == NULL) return 0;
    return afe->afe_iface->get_feed_chunksize(afe->afe_data);
}

void audio_afe_set_output_callback(audio_afe_t* afe, afe_output_callback_t callback)
{
    if (afe == NULL) return;
    afe->output_callback = callback;
}

int audio_afe_get_audio_level(void)
{
    if (g_afe_instance == NULL) return 0;
    return g_afe_instance->last_audio_level;
}

i2s_chan_handle_t audio_afe_get_tx_handle(audio_afe_t* afe)
{
    if (afe == NULL) return NULL;
    return afe->tx_handle;
}

void audio_afe_pause_rx(audio_afe_t* afe)
{
    if (afe == NULL || afe->rx_handle == NULL) return;
    i2s_channel_disable(afe->rx_handle);
    ESP_LOGI(TAG, "RX 已暂停");
}

void audio_afe_resume_rx(audio_afe_t* afe)
{
    if (afe == NULL || afe->rx_handle == NULL) return;
    i2s_channel_enable(afe->rx_handle);
    ESP_LOGI(TAG, "RX 已恢复");
}

