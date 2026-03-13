/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_afe_sr_models.h"

#include "model_path.h"
#include "esp_vad.h"
#include "esp_process_sdkconfig.h"
// #include "bsp/esp-bsp.h"
// #include "file_iterator.h"
#include "esp_sparkbot_bsp.h"
#include "bsp_board_extra.h"

#include "baidu.h"
#include "app_audio_record.h"
#include "mmap_generate_audio.h"
#include "mmap_generate_storage.h"
#include "app_eeprom.h"
#include "app_animation.h"
#include "app_wifi.h"
// #include "ui.h"

#define I2S_CHANNEL_NUM     (1)
#define MAX_AUDIO_INPUT_LENGTH  16000 * 2 * 8   // 8 seconds audio
#define MIN_AUDIO_INPUT_LENGTH  16000 * 2 * 600 / 1000
#define SPIFFS_BASE             "/audio"

#define AUDIO_STOP_BIT          BIT0
#define AUDIO_CHAT_BIT          BIT1

#define USE_PROTOCAL 1

static const char *TAG = "app_sr";

typedef enum {
    MODE_IDLE,
    MODE_CHAT,
    MODE_IMAGE,
} running_mode_t;

typedef struct {
    size_t len;
    uint8_t *wav;
} audio_data_t;

static EventGroupHandle_t   g_stt_event_group;
static QueueHandle_t        g_audio_chat_queue  = NULL;
static QueueHandle_t        g_audio_tts_queue   = NULL;
static QueueHandle_t        g_queue_audio_play  = NULL;
static running_mode_t       g_running_mode      = MODE_CHAT;

static const esp_afe_sr_iface_t *afe_handle     = NULL;
static QueueHandle_t            g_result_que    = NULL;
static srmodel_list_t           *models         = NULL;

static esp_codec_dev_handle_t   spk_codec_dev   = NULL;
// static file_iterator_instance_t *file_iterator  = NULL;

static TaskHandle_t xFeedHandle;
static TaskHandle_t xDetectHandle;
static mmap_assets_handle_t asset_audio;

static bool                     g_voice_recording       = false;
static bool                     g_audio_playing         = false;
static bool                     g_face_updated          = false;
static size_t                   g_stt_recorded_length   = 0;
static int16_t                  *g_audio_record_buf     = NULL;
bool wait_speech_flag = false;/////////////////////////改成全局，播放完音频置成true.
static void audio_play_task(void *arg)
{
    spk_codec_dev = bsp_extra_audio_codec_speaker_init();
    esp_codec_dev_set_out_vol(spk_codec_dev, 50);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate        = 16000,
        .channel            = 1,
        .bits_per_sample    = 16,
    };

    esp_codec_dev_open(spk_codec_dev, &fs);
    audio_data_t audio_data = {0};
    printf("Create audio play task\n");

    while (xQueueReceive(g_queue_audio_play, &audio_data, portMAX_DELAY) == pdTRUE) {
        // ESP_LOGW(TAG, "audio_play_task: %d, wav: %p", audio_data.len, audio_data.wav);
        int res = esp_codec_dev_write(spk_codec_dev, audio_data.wav, audio_data.len);
        // ESP_LOGI(TAG, "esp_codec_dev_write %d", res);
        free(audio_data.wav);
    }

    esp_codec_dev_close(spk_codec_dev);
    vTaskDelete(NULL);
}

esp_err_t audio_play(uint8_t *wav, size_t len)
{
    audio_data_t audio_data = {
        .len = len,
        .wav = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    };

    if (audio_data.wav == NULL) {
        ESP_LOGE(TAG, "heap_caps_malloc failed");
        return ESP_OK;
    }

    memcpy(audio_data.wav, wav, len);
    printf("audio_playing: %d bytes\n", len);
    xQueueSend(g_queue_audio_play, &audio_data, portMAX_DELAY);
    
    return ESP_OK;
}

static void audio_feed_task(void *pvParam)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) pvParam;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // ESP_LOGI(TAG, "audio_chunksize=%d, feed_channel=%d", audio_chunksize, 3);
    /* Allocate audio buffer and check for result */
    int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (NULL == audio_buffer) {
        esp_system_abort("No mem for audio buffer");
    }

    /* Set the input gain of the microphone, increase the gain if the input volume is too low */
    esp_codec_dev_handle_t mic_codec_dev = bsp_extra_audio_codec_microphone_init();

    esp_codec_dev_sample_info_t fs = {
        .sample_rate        = 16000,
        .channel            = 1,
        .bits_per_sample    = 16,
    };
    esp_codec_dev_open(mic_codec_dev, &fs);
    /* Set the input gain of the microphone, increase the gain if the input volume is too low */
    esp_codec_dev_set_in_gain(mic_codec_dev, 35.0);
    printf("create audio feed task\n");
    while (true) 
    {
        /* Read audio data from I2S bus */
        esp_codec_dev_read(mic_codec_dev, audio_buffer, audio_chunksize * I2S_CHANNEL_NUM * sizeof(int16_t));

        /* Send audio data to recording */
        if (g_voice_recording) 
        {

            /* Stop when 8 seconds is full */
            if (g_stt_recorded_length >= MAX_AUDIO_INPUT_LENGTH) {
                audio_record_state_t result = AUDIO_VAD_END;
                xQueueSend(g_result_que, &result, 10);
            }

            /* Data length to write */
            size_t bytes_to_write = audio_chunksize * sizeof(int16_t);

            /* Check if exceding max recording length (should not happen by theory) */
            if (g_stt_recorded_length + bytes_to_write > MAX_AUDIO_INPUT_LENGTH) {
                bytes_to_write = MAX_AUDIO_INPUT_LENGTH - g_stt_recorded_length;
            }

            if (g_stt_recorded_length + bytes_to_write <= MAX_AUDIO_INPUT_LENGTH) {
                memcpy(g_audio_record_buf + g_stt_recorded_length / 2, audio_buffer, bytes_to_write);
                /* Update recorded length */
                g_stt_recorded_length += bytes_to_write;
                // ESP_LOGI(TAG, "Recording: %d bytes written, total: %d", bytes_to_write, g_stt_recorded_length);
            } else {
                /* write to the end of the buffer, then wrap around and write the rest of the buffer. */
                ESP_LOGW(TAG, "Buffer full, unable to write more audio data");
                audio_record_state_t result = AUDIO_VAD_END;
                xQueueSend(g_result_que, &result, 10);
            }
        }

        /* Channel Adjust */
        // for (int  i = audio_chunksize - 1; i >= 0; i--) {
        //     audio_buffer[i * 3 + 2] = 0;
        //     audio_buffer[i * 3 + 1] = audio_buffer[i];
        //     audio_buffer[i * 3 + 0] = audio_buffer[i];
        // }
        for (int  i = audio_chunksize - 1; i >= 0; i--) {
            audio_buffer[i * 2 + 1] = 0;
            audio_buffer[i * 2 + 0] = audio_buffer[i];
        }

        /* Feed samples of an audio stream to the AFE_SR */
        afe_handle->feed(afe_data, audio_buffer);
    }

    /* Clean up if audio feed ends */
    afe_handle->destroy(afe_data);

    /* Task never returns */
    vTaskDelete(NULL);
}

static void audio_detect_task(void *pvParam)
{
    
    bool detect_flag = false;
    vad_state_t vad_state = VAD_SILENCE;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) pvParam;

    /* Check audio data chunksize */
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    // ESP_LOGI(TAG, "------------create detect task------------\n");
    printf("------------create detect task------------\n");
    ESP_LOGI(TAG, "afe_chunksize: %d", afe_chunksize);
    int cnt = 0;

    while (true) 
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        vad_state = res->vad_state;
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "fetch error!");
            continue;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            // ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_GREEN) "Wakeword detected");
            ESP_LOGI(TAG, "Wakeword detected");
            audio_record_state_t result = AUDIO_WAKENET_START;
            xQueueSend(g_result_que, &result, 10);
            /* Update face UI */
            // ui_send_sys_event(ui_face, LV_EVENT_FACE_ASK, NULL);
        } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            // ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_GREEN) "Channel verified");
            printf("Channel verified\n");
            afe_handle->disable_wakenet(afe_data);
            wait_speech_flag = true;
        }
        // else if (vad_state == VAD_SPEECH) {
        //     // vTaskDelay(pdMS_TO_TICKS(500));
        //     detect_flag = true;

        // }
        if (wait_speech_flag && app_get_wifi_connected_state()) 
        {
            if (vad_state == VAD_SPEECH) 
            {
                ESP_LOGI(TAG, "speech detected, vad start");
                app_animation_play(2); // 播放猫爪动画
                detect_flag = true;
                wait_speech_flag = false;
                audio_record_state_t result = AUDIO_VAD_START;
                xQueueSend(g_result_que, &result, 10);
            }
            else
            {
                if(++cnt>2500)
                {
                    wait_speech_flag = false;
                    ESP_LOGI(TAG, "speech close");
                    cnt = 0;
                }
                if(cnt%500==0)
                {
                    ESP_LOGI(TAG, "no voice");
                }
                vTaskDelay(1);
            }
        }

        /* VAD detect */
        if (detect_flag) 
        {
        
            if (vad_state == VAD_SILENCE) 
            {
                ESP_LOGI(TAG, "wait for more audio");

                // 每隔 100 毫秒检查一次 vad_state
                for (int i = 0; i < 20; i++) 
                {
                    vTaskDelay(100);
                    res = afe_handle->fetch(afe_data);
                    vad_state = res->vad_state;

                    if (vad_state != VAD_SILENCE) 
                    {
                        break; // 如果不再是静音状态，退出循环
                    }
                }

                res = afe_handle->fetch(afe_data);
                vad_state = res->vad_state;
                if (vad_state == VAD_SILENCE) 
                {
                    ESP_LOGI(TAG, "vad state: VAD_SILENCE");
                    audio_record_state_t result = AUDIO_VAD_END;
                    xQueueSend(g_result_que, &result, 10);
                    /* Update face UI */
                    // ui_send_sys_event(ui_face, LV_EVENT_FACE_THINK, NULL);

                    afe_handle->enable_wakenet(afe_data);
                    detect_flag = false;
                }
                continue;
            }
        }
    }

    /* Clean up if audio feed ends */
    afe_handle->destroy(afe_data);

    /* Task never returns */
    vTaskDelete(NULL);
}

void audio_record_task(void *pvParam)
{
    printf("audio_record_task start\n");
    while (true) {
        audio_record_state_t result;
        if (xQueueReceive(g_result_que, &result, portMAX_DELAY) == pdTRUE) {
            switch (result) {
            case AUDIO_PLAY_MUYU: {
                g_audio_playing = false;
                void *audio = (void *)mmap_assets_get_mem(asset_audio, MMAP_STORAGE_MUYU_WAV);
                uint32_t len = mmap_assets_get_size(asset_audio, MMAP_STORAGE_MUYU_WAV);
                esp_codec_dev_write(spk_codec_dev, audio, len);
                break;
            }
            case AUDIO_WAKENET_START: {
                ESP_LOGI(TAG, "wakenet start");
                g_audio_playing = false;
                void *audio = (void *)mmap_assets_get_mem(asset_audio, MMAP_STORAGE_JIAO_WO_GAN_MA_WAV);
                uint32_t len = mmap_assets_get_size(asset_audio, MMAP_STORAGE_JIAO_WO_GAN_MA_WAV);
                esp_codec_dev_write(spk_codec_dev, audio, len);
                break;
            }
            case AUDIO_WAKENET_END: {
                ESP_LOGI(TAG, "timeout");
                break;
            }
            case AUDIO_VAD_START: {
                ESP_LOGI(TAG, "VAD detect start");
                g_voice_recording = true;
                break;
            }
            case AUDIO_VAD_END: {
                ESP_LOGI(TAG, "VAD detect done");
                /* Ensure audio length is > 600ms */

                g_voice_recording = false;
                if (g_stt_recorded_length > MIN_AUDIO_INPUT_LENGTH) {
                    /* Run STT Task */
                    xEventGroupSetBits(g_stt_event_group, AUDIO_CHAT_BIT);
                }
                break;
            }
            default:
                break;
            }
        }
    }
    vTaskDelete(NULL);
}

void app_stt_task(void *arg)
{
    // ESP_LOGI(TAG, "app_stt_task start");
    printf("app_stt_task start\n");
    while (1) {

        xEventGroupWaitBits(g_stt_event_group, AUDIO_CHAT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        baidu_asr_send_audio(g_audio_record_buf, g_stt_recorded_length, g_stt_recorded_length);
        // audio_play((uint8_t *)g_audio_record_buf, g_stt_recorded_length);
        g_stt_recorded_length = 0;

        /* Receive text from stt */
        char *message_content = NULL;
        baidu_asr_recv_text(&message_content);

        if (message_content == NULL) {
            ESP_LOGE(TAG, "message_content is NULL");
            // ui_send_sys_event(ui_face, LV_EVENT_FACE_LOOK, NULL);
            continue;
        }

        /* Send text to tts task */
        QueueHandle_t audio_chat_queue = NULL;

        if (g_running_mode == MODE_CHAT) {
            audio_chat_queue = g_audio_chat_queue;
        }

        if (xQueueSend(audio_chat_queue, &message_content, 0) == pdFALSE) {
            free(message_content);
            message_content = NULL;
        }

        xEventGroupSetBits(g_stt_event_group, AUDIO_STOP_BIT);
    }

    ESP_LOGI(TAG, "app_stt_task end");
    vTaskDelete(NULL);
}

void audio_chat_task(void *arg)
{
    ESP_LOGI(TAG, "audio_chat_task start");
#if USE_PROTOCAL == 0   
    bsp_uart_init();
#endif
    char *chat_data = NULL;
    
    while (1) 
    {
        char system_data[300] = "你是一个叫乐鑫的语音助手, 替人答疑解惑。回答尽可能简短";
        if (xQueueReceive(g_audio_chat_queue, &chat_data, portMAX_DELAY) != pdTRUE) 
        {
            continue;
        }

        // 健壮性检查：确保接收到的指针是有效的
        if (chat_data == NULL) {
            ESP_LOGW(TAG, "Received a NULL pointer from queue.");
            continue;
        }
#if USE_PROTOCAL == 0  
        if (strstr(chat_data, "前进") != NULL || strstr(chat_data, "冲") != NULL) 
        {
            ESP_LOGI(TAG, "Command: Forward");
            tracked_chassis_motion_control("x0.0y1.0");              // 开始前进（正确指令）
            vTaskDelay(pdMS_TO_TICKS(1000));                         // 持续运动1秒
            tracked_chassis_motion_control("x0.0y0.0");              // 停止运动
        }
        else if (strstr(chat_data, "后退") != NULL || strstr(chat_data, "退") != NULL) 
        {
            ESP_LOGI(TAG, "Command: Backward");
            tracked_chassis_motion_control("x0.0y-1.0");             // 开始后退（正确指令）
            vTaskDelay(pdMS_TO_TICKS(1000));                         // 持续运动1秒
            tracked_chassis_motion_control("x0.0y0.0");              // 停止运动
        }
        else if (strstr(chat_data, "左转") != NULL || strstr(chat_data, "左") != NULL) 
        {
            ESP_LOGI(TAG, "Command: Turn Left");
            tracked_chassis_motion_control("x-1.0y0.0");             // 开始左转（正确指令）
            vTaskDelay(pdMS_TO_TICKS(1000));                         // 持续运动1秒
            tracked_chassis_motion_control("x0.0y0.0");              // 停止运动
        }
        else if (strstr(chat_data, "右转") != NULL || strstr(chat_data, "右") != NULL) 
        {
            ESP_LOGI(TAG, "Command: Turn Right");
            tracked_chassis_motion_control("x1.0y0.0");              // 开始右转（正确指令）
            vTaskDelay(pdMS_TO_TICKS(1000));                         // 持续运动1秒
            tracked_chassis_motion_control("x0.0y0.0");              // 停止运动
        }
        else if (strstr(chat_data, "上网") != NULL)
        {
            printf("%c%c%c", 0XA1, 0XA2, 0XA3);
        }
        
#elif USE_PROTOCAL == 1
        char cache[256] = {0};
        if(at24c08_read_bytes(0x01, (uint8_t*)cache, 165) == ESP_OK)
        {
            
            strncpy(system_data, cache, 165);
        }
        
#endif
        printf("字节长度：%d", strlen(cache));
        printf("设定信息：%s", system_data);
        baidu_chatbot_send_request(system_data, chat_data);
        free(chat_data);
        chat_data = NULL;

        char response_data[512] = {0};
        if (baidu_chatbot_recv_response(response_data, sizeof(response_data)) != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to receive or parse response from LLM.");
            continue;
        }

        // ESP_LOGI(TAG, "response_data: %s", response_data);

        /* Send text to tts task */
        QueueHandle_t audio_chat_queue = NULL;

        if (g_running_mode == MODE_CHAT) {
            audio_chat_queue = g_audio_tts_queue;
        }

        if (g_running_mode == MODE_CHAT && g_audio_tts_queue != NULL) 
        {
            char *tts_data = strdup(response_data);
            if (tts_data) 
            {
                if (xQueueSend(audio_chat_queue, &tts_data, pdMS_TO_TICKS(100)) == pdFALSE) 
                {
                    free(tts_data);
                }
            }
        }
        chat_data = NULL; // 将指针置空，防止后续误用
    }

    ESP_LOGI(TAG, "audio_chat_task end");
    vTaskDelete(NULL);
}

void audio_tts_task(void *arg)
{
    ESP_LOGI(TAG, "audio_tts_task start");

    char *text = NULL;
    while (xQueueReceive(g_audio_tts_queue, &text, portMAX_DELAY) == pdTRUE) 
    {
        ESP_LOGI(TAG, "audio_tts_task, size: %d, text: %s", strlen(text), text);
        // printf("response text: %s\n", text);//
        g_audio_playing = true;

        uint8_t person_tone = 4;
#if USE_PROTOCAL == 1
        if(at24c08_read_bytes(0x00, &person_tone, 1) != ESP_OK) 
        {
            ESP_LOGE(TAG, "Failed to read person_tone from EEPROM, using default.");
            person_tone = 4; // Default tone
        }
        printf("person_tone: %d\n", person_tone);
#endif
        baidu_tts_send_text(text, person_tone);
        free(text);
        text = NULL;
        app_animation_play(6);

        uint8_t *data = NULL;

        g_face_updated = true;
        while (1) 
        {
            size_t len = 0;
            size_t total_len = 0;

            if (baidu_tts_recv_audio(&data, &len, &total_len, g_audio_playing)) 
            {
                if (data == NULL) 
                {
                    ESP_LOGI(TAG, "tts audio data received is NULL");
                    break;
                } 
                else 
                {
                    // ESP_LOGI(TAG, "len: %d, total_len: %d", len, total_len);
                    if (g_face_updated) {
                        /* Update face UI */
                        // ui_send_sys_event(ui_face, LV_EVENT_FACE_SPEAK, NULL);
                        g_face_updated = false;
                    }
                    audio_play(data, len);
                    free(data);
                    
                    data = NULL;
                }
            } 
            else 
            {
                ESP_LOGI(TAG, "baidu_tts_recv_audio done");
                break;
            }
        }

        g_audio_playing = false;

        vTaskDelay(3000);
        if (!g_audio_playing) 
        {
            app_animation_play(3);
            wait_speech_flag = true;
        }

        ESP_LOGI(TAG, "heap after audio play, internal current: %d, minimum: %d, total current: %d, minimum: %d",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                 heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                 esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    }

    ESP_LOGI(TAG, "audio_tts_task end");
    vTaskDelete(NULL);
}

void app_sr_paly_muyu()
{
    audio_record_state_t result = AUDIO_PLAY_MUYU;
    if(g_result_que){
        xQueueSend(g_result_que, &result, 10);
    }
}

static void app_sr_mmap_audio()
{
    const mmap_assets_config_t config = {
        .partition_label = "storage",
        .max_files = MMAP_AUDIO_FILES,
        .checksum = MMAP_AUDIO_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = true,
        },
    };

    mmap_assets_new(&config, &asset_audio);
    ESP_LOGI(TAG, "stored_files:%d", mmap_assets_get_stored_files(asset_audio));
}

esp_err_t app_sr_start(void)
{
    /* SPIFF init */
    // bsp_spiffs_mount();
    app_sr_mmap_audio();

    /* Audio buffer init */
    g_audio_record_buf = heap_caps_malloc(MAX_AUDIO_INPUT_LENGTH + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    models = esp_srmodel_init("model");

    afe_handle = &ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.total_ch_num = 2;

    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);
    ESP_LOGI(TAG, "load wakenet:%s", afe_config.wakenet_model_name);

    g_result_que = xQueueCreate(1, sizeof(audio_record_state_t));
    ESP_RETURN_ON_FALSE(NULL != g_result_que, ESP_ERR_NO_MEM, TAG, "Failed create result queue");

    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "Feed Task", 4 * 1024, afe_data, 5, &xFeedHandle, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "Detect Task", 6 * 1024, afe_data, 5, &xDetectHandle, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(audio_record_task, "Audio Record Task", 4 * 1024, g_result_que, 1, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio handler task");

    /*
    Step 1: Send captured audio data to Speech-to-Text (STT) server for processing.
    Step 2: Receive and handle the text response from Baidu Cloud STT service.
    Step 3: Forward the received text response to the Text-to-Speech (TTS) server.
    Step 4: Forward the audio response to audio_play_task.
    */
    g_audio_chat_queue = xQueueCreate(16, sizeof(char *));
    g_stt_event_group = xEventGroupCreate();
    g_audio_tts_queue = xQueueCreate(16, sizeof(char *));
    g_queue_audio_play = xQueueCreate(1, sizeof(audio_data_t));

    xTaskCreatePinnedToCore(app_stt_task, "audio_stt", 1024 * 4, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(audio_chat_task, "audio_chat", 1024 * 6, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audio_tts_task, "audio_tts", 1024 * 6, NULL, 6, NULL, 1);
    xTaskCreate(audio_play_task, "audio_play_task", 1024 * 5, NULL, 15, NULL);

    return ESP_OK;
}

// esp_err_t http_event_handler(esp_http_client_event_t *evt) {
//     static char* response_buffer = NULL;
//     static size_t response_len = 0;

//     switch (evt->event_id) {
//         case HTTP_EVENT_ON_DATA:
//             // 动态分配或扩容缓冲区以存储响应数据
//             if (!response_buffer) {
//                 response_buffer = (char*) malloc(evt->data_len + 1);
//                 if (response_buffer) {
//                     memcpy(response_buffer, evt->data, evt->data_len);
//                     response_buffer[evt->data_len] = '\0';
//                     response_len = evt->data_len;
//                 }
//             } else {
//                 char* new_buf = (char*) realloc(response_buffer, response_len + evt->data_len + 1);
//                 if (new_buf) {
//                     response_buffer = new_buf;
//                     memcpy(response_buffer + response_len, evt->data, evt->data_len);
//                     response_len += evt->data_len;
//                     response_buffer[response_len] = '\0';
//                 }
//             }
//             break;

//         case HTTP_EVENT_ON_FINISH:
//             ESP_LOGI(TAG, "HTTP传输完成");
//             if (response_buffer) {
//                 ESP_LOGI(TAG, "完整响应: %s", response_buffer);
//                 // 解析JSON响应
//                 cJSON *root = cJSON_Parse(response_buffer);
//                 if (root) {
//                     cJSON *result = cJSON_GetObjectItem(root, "result");
//                     if (cJSON_IsString(result)) {
//                         ESP_LOGI(TAG, "=== AI回复: %s ===", result->valuestring);
//                     } else {
//                         ESP_LOGE(TAG, "未找到'result'字段或类型不正确");
//                     }
//                     cJSON_Delete(root);
//                 } else {
//                     ESP_LOGE(TAG, "JSON解析失败");
//                 }
//                 free(response_buffer);
//                 response_buffer = NULL;
//                 response_len = 0;
//             }
//             break;

//         case HTTP_EVENT_DISCONNECTED:
//             ESP_LOGI(TAG, "HTTP连接断开");
//             if (response_buffer) {
//                 free(response_buffer);
//                 response_buffer = NULL;
//                 response_len = 0;
//             }
//             break;

//         default:
//             break;
//     }
//     return ESP_OK;
// }
// #define ACCESS_TOKEN "24.f011da2a438f6e0af7bc092151ec402e.2592000.1767527264.282335-121194119"
// /*
//  * 向百度大模型发送对话请求的任务函数
//  */
// void app_sr_start(void *pvParameters) {
//     ESP_LOGI(TAG, "百度大模型请求任务开始");

//     // 1. 构造请求数据（JSON格式）
//     cJSON *request_root = cJSON_CreateObject();
//     cJSON_AddStringToObject(request_root, "message", "你好，请简单介绍一下你自己。"); // 用户输入的消息
//     // 根据您调用的具体百度大模型API文档，可能还需要添加其他必填字段，如"user_id"等
//     char *post_data = cJSON_PrintUnformatted(request_root);
//     cJSON_Delete(request_root);

//     ESP_LOGI(TAG, "请求数据: %s", post_data);

//     // 2. 配置HTTP客户端
//     char url[256];
//     // 注意：请将 URL 替换为百度大模型服务（如文心一言）您所使用的实际API端点
//     snprintf(url, sizeof(url), "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/xxx?access_token=%s", ACCESS_TOKEN);

//     esp_http_client_config_t config = {
//         .url = url,
//         .method = HTTP_METHOD_POST,
//         .event_handler = http_event_handler,
//         .timeout_ms = 10000, // 设置超时时间（毫秒）
//     };

//     // 3. 初始化并执行HTTP请求
//     esp_http_client_handle_t client = esp_http_client_init(&config);
//     esp_http_client_set_header(client, "Content-Type", "application/json");
//     esp_http_client_set_post_field(client, post_data, strlen(post_data));

//     esp_err_t err = esp_http_client_perform(client);
//     if (err == ESP_OK) {
//         int status_code = esp_http_client_get_status_code(client);
//         ESP_LOGI(TAG, "HTTP请求状态码: %d", status_code);
//         if (status_code >= 200 && status_code < 300) {
//             ESP_LOGI(TAG, "请求成功！");
//         } else {
//             ESP_LOGE(TAG, "请求可能失败，状态码: %d", status_code);
//         }
//     } else {
//         ESP_LOGE(TAG, "HTTP请求失败: %s", esp_err_to_name(err));
//     }

//     // 4. 清理资源
//     esp_http_client_cleanup(client);
//     free(post_data);

//     ESP_LOGI(TAG, "百度大模型请求任务结束,10秒后重启...");
//     vTaskDelay(10000);

//     vTaskDelete(NULL);
// }