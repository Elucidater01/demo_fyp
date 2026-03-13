/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>

#include "esp_log.h"
#include "esp_event.h"
#include "esp_tls.h"

#include "cJSON.h"
#include "esp_http_client.h"
// #include "app_eeprom.h"
#include "baidu.h"

static const char *TAG = "baidu_speech";


esp_http_client_handle_t baidu_erniebot_http_handle = NULL;
static char *response_buf = NULL;
static int response_len = 0;
/*
 * Parses the data response from the Baidu API at the following endpoint:
 * http://vop.baidu.com/server_api
 *
 * This function handles the response format and extracts relevant information
 * for further processing in the application.
 */
static char *baidu_stt_response_parse(const char *data, size_t len)
{
    cJSON *root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGI(TAG, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return NULL; 
    }

    cJSON *result_node = cJSON_GetObjectItem(root, "result");
    if (result_node == NULL || result_node->type != cJSON_Array) {
        ESP_LOGI(TAG, "Error: 'result' key not found in JSON.\n");
        ESP_LOGI(TAG, "Data received: %s", data);
        cJSON_Delete(root);
        return NULL; 
    }

    cJSON *first_result = cJSON_GetArrayItem(result_node, 0);
    if (first_result == NULL || first_result->type != cJSON_String) {
        ESP_LOGI(TAG, "Error: First item in 'result' is not a string.\n");
        ESP_LOGI(TAG, "Data received: %s", data);
        cJSON_Delete(root);
        return NULL;
    }

    char *result_value = NULL;
    asprintf(&result_value, "%s", first_result->valuestring);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "result_value: %s", result_value);

    return result_value;
}

/*
 * Parses the data response from the Baidu API at the following endpoint:
 * https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/ernie-lite-8k
 *
 * This function handles the response format and extracts relevant information
 * for further processing in the application.
 */
static char *baidu_content_response_parse(const char *data, size_t len)
{
    cJSON *root = cJSON_Parse(data);
    if (root == NULL) {
        printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        goto err;
    }

    cJSON *result_node = cJSON_GetObjectItem(root, "result");
    if (result_node == NULL || !cJSON_IsString(result_node)) {
        printf("Error: .\n");
        goto err;
    }

    char *result_value = strdup(result_node->valuestring);

    cJSON_Delete(root);
    return result_value;

err:
    cJSON_Delete(root);
    return NULL;
}

static char *baidu_content_stream_response_parse(const char *data, size_t len)
{
    cJSON *root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Error parsing JSON: %s", cJSON_GetErrorPtr());
        goto err;
    }

    cJSON *is_end = cJSON_GetObjectItem(root, "is_end");
    if (is_end == NULL || !cJSON_IsBool(is_end)) {
        ESP_LOGE(TAG, "Error: response is not stream");
        goto err;
    }

    if (cJSON_IsTrue(is_end)) {
        ESP_LOGI(TAG, "response is end");
        goto err;
    }

    cJSON *result_node = cJSON_GetObjectItem(root, "result");
    if (result_node == NULL || !cJSON_IsString(result_node)) {
        ESP_LOGE(TAG, "Error: 'result' key not found or not a string.");
        goto err;
    }

    char *result_value = strdup(result_node->valuestring);

    cJSON_Delete(root);
    return result_value;

err:
    cJSON_Delete(root);
    return NULL;
}

// #define BAIDUBCE_MESSAGE_FORMAT "{\"model\":\"ernie-4.5-turbo-128k\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"web_search\":{\"enable\":false,\"enable_citation\":false,\"enable_trace\":false},\"plugin_options\":{}}"
// #define BAIDU_LLM_URL            "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/ernie-lite-8k?access_token=%s"
// #define BAIDU_LLM_URL           "https://qianfan.baidubce.com/v2/chat/completions"
// {
//     "request_id": "fa82ebdc-2b7c-4b3f-9b03-72ac9d7a9895",
//     "conversation_id": "9594e2af-e196-455a-83f4-d8110cc07f3a"
// }
#define BAIDU_LLM_URL           "https://qianfan.baidubce.com/v2/app/conversation/runs"
#define API_KEY                

esp_err_t baidu_chatbot_send_request(char *system_content, char *user_content)
{

    // return ESP_OK; 
        if (baidu_erniebot_http_handle) {
        esp_http_client_cleanup(baidu_erniebot_http_handle);
        baidu_erniebot_http_handle = NULL;
    }

    esp_http_client_config_t config = {
        .url = BAIDU_LLM_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
    };

    baidu_erniebot_http_handle = esp_http_client_init(&config);
    if (!baidu_erniebot_http_handle) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    // headers
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", API_KEY);
    esp_http_client_set_header(baidu_erniebot_http_handle, "Content-Type", "application/json");//调换了顺序
    esp_http_client_set_header(baidu_erniebot_http_handle, "Authorization", auth_header);//调换了顺序
    // esp_http_client_set_header(baidu_erniebot_http_handle, "Accept", "application/json");

    // 构建POST数据（使用智能体API要求的格式）
    char *post_data = NULL;
    // 替换为您的智能体App ID
    char *app_id = "91abe41c-00a2-4724-9694-a7bec77a6612";

    // 使用智能体API的JSON结构（app_id + stream，而非model/messages）
    // 使用智能体API的JSON结构（app_id + query + stream）
    asprintf(&post_data, 
    "{\"app_id\":\"%s\",\"query\":\"%s(你的设定：%s,回答不要分点，尽可能简短不超20字)\",\"conversation_id\":\"9594e2af-e196-455a-83f4-d8110cc07f3a\",\"stream\":%s}",
    app_id,  // 您的应用ID
    user_content ? user_content : "",        // 用户查询内容
    system_content ? system_content : "",
    "false"                                   // 流式模式关闭
    );

    if (!post_data) {
        ESP_LOGE(TAG, "Failed to allocate post_data");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_open(baidu_erniebot_http_handle, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(post_data);
        return err;
    }

    int written = esp_http_client_write(baidu_erniebot_http_handle, post_data, strlen(post_data));
    free(post_data);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to write data");
        esp_http_client_close(baidu_erniebot_http_handle);
        return ESP_FAIL;
    }

    // 清理旧响应 buffer
    if (response_buf) {
        free(response_buf);
        response_buf = NULL;
        response_len = 0;
    }

    // --- 读取响应 ---
    int64_t content_length = esp_http_client_fetch_headers(baidu_erniebot_http_handle);
    int status_code = esp_http_client_get_status_code(baidu_erniebot_http_handle);
    ESP_LOGI(TAG, "HTTP status = %d, content_length = %lld", status_code, (long long)content_length);

    if (content_length > 0) {
        // 有 Content-Length
        response_buf = calloc(1, content_length + 1);
        if (!response_buf) {
            ESP_LOGE(TAG, "No memory for response_buf");
            esp_http_client_close(baidu_erniebot_http_handle);
            return ESP_ERR_NO_MEM;
        }
        int total_read = 0;
        while (total_read < content_length) {
            int r = esp_http_client_read(baidu_erniebot_http_handle, response_buf + total_read, content_length - total_read);
            if (r <= 0) break;
            total_read += r;
        }
        response_len = total_read;
        response_buf[response_len] = '\0';
    } else {
        // chunked 或未知长度
        int buf_size = 1024;
        response_buf = calloc(1, buf_size);
        if (!response_buf) {
            ESP_LOGE(TAG, "No memory for response_buf");
            esp_http_client_close(baidu_erniebot_http_handle);
            return ESP_ERR_NO_MEM;
        }
        int total_read = 0;
        while (1) {
            int r = esp_http_client_read(baidu_erniebot_http_handle, response_buf + total_read, buf_size - total_read - 1);
            if (r <= 0) break;
            total_read += r;
            if (total_read >= buf_size - 1) {
                buf_size *= 2;
                response_buf = realloc(response_buf, buf_size);
                if (!response_buf) {
                    ESP_LOGE(TAG, "Realloc failed");
                    esp_http_client_close(baidu_erniebot_http_handle);
                    return ESP_ERR_NO_MEM;
                }
            }
        }
        response_len = total_read;
        response_buf[response_len] = '\0';
    }

    esp_http_client_close(baidu_erniebot_http_handle);
    ESP_LOGI(TAG, "Raw Response (%d bytes): %s", response_len, response_buf ? response_buf : "");

    return ESP_OK;
}

esp_err_t baidu_chatbot_recv_response(char *response_data, size_t output_size)
{
    if (!response_buf) {
        ESP_LOGE(TAG, "No response available");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(response_buf);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_FAIL;
    
    // 直接从根对象获取answer字段
    cJSON *answer = cJSON_GetObjectItem(root, "answer");
    if (answer && cJSON_IsString(answer)) {
        snprintf(response_data, output_size, "%s", answer->valuestring);
        ESP_LOGI(TAG, "LLM Response: %s", response_data);
        ret = ESP_OK;
    } else 
    {
        // 如果answer字段不存在，尝试从content数组中查找最终的文本输出
        cJSON *content = cJSON_GetObjectItem(root, "content");
        if (content && cJSON_IsArray(content)) {
            int array_size = cJSON_GetArraySize(content);
            
            // 从后往前遍历数组，找到最后一个content_type为"text"的项
            for (int i = array_size - 1; i >= 0; i--) {
                cJSON *item = cJSON_GetArrayItem(content, i);
                if (item) {
                    cJSON *content_type = cJSON_GetObjectItem(item, "content_type");
                    if (content_type && cJSON_IsString(content_type) && 
                        strcmp(content_type->valuestring, "text") == 0) {
                        
                        cJSON *outputs = cJSON_GetObjectItem(item, "outputs");
                        if (outputs) {
                            cJSON *text = cJSON_GetObjectItem(outputs, "text");
                            if (text && cJSON_IsString(text)) {
                                snprintf(response_data, output_size, "%s", text->valuestring);
                                ESP_LOGI(TAG, "LLM Response (from content): %s", response_data);
                                ret = ESP_OK;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse answer content");
    }

    cJSON_Delete(root);
    return ret;
}

/*
* The following code sends an audio input to Baidu's speech-to-text (STT) service,
* receives a text response, and processes it.
*/
#define BAIDUBCE_STT_URL            "http://vop.baidu.com/server_api?dev_pid=1537&cuid=F2sreBeZNdY2jWwlUwbTxPy8R1gFMzEL&token=%s"
#define AUDIO_TOKEN                 
#define CUID "F2sreBeZNdY2jWwlUwbTxPy8R1gFMzEL"
static esp_http_client_handle_t g_baidu_asr_http_handle = NULL;

esp_err_t baidu_asr_send_audio(const int16_t *audio, size_t len, size_t total_len)
{
    // char *url_str = NULL;
    // asprintf(&url_str, BAIDUBCE_STT_URL, AUDIO_TOKEN);
    // printf("---------------Baidu ASR URL: %s\n", url_str);

    // if (!g_baidu_asr_http_handle) {
    //     esp_http_client_config_t config = {
    //         .url = url_str,
    //         .method = HTTP_METHOD_POST,
    //         .skip_cert_common_name_check = true,
    //         .buffer_size_tx = 1460,
    //         .timeout_ms = 30000,
    //     };

    //     /* Set the headers */
    //     g_baidu_asr_http_handle = esp_http_client_init(&config);
    //     esp_http_client_set_header(g_baidu_asr_http_handle, "Content-Type", "audio/pcm;rate=16000");

    //     esp_http_client_open(g_baidu_asr_http_handle, total_len);
    // }

    // for (int written_len = 0; written_len < len; ) 
    // {
    //     written_len += esp_http_client_write(g_baidu_asr_http_handle, (char *)(audio + written_len), 1460);
    //     // ESP_LOG_BUFFER_HEXDUMP("HTTP_CLIENT", (char*)(audio + written_len), 14600, ESP_LOG_INFO);
    //     // printf("audio data:%s\n", (char *)(audio + written_len));
    //     printf("written_len:%d\n", written_len);
    // }

    // /* 打印audio */
    // printf("Baidu ASR request sent, audio len: %d\n", len);

    // return ESP_OK;
    esp_err_t err = ESP_FAIL;
    ESP_LOGI(TAG, "待发送音频数据长度: %d 字节", len);
    if (len > 16) {
        ESP_LOGI(TAG, "音频数据样本 (前16字节): %02x %02x %02x %02x %02x %02x %02x %02x ...",
                 ((uint8_t*)audio)[0], ((uint8_t*)audio)[1], ((uint8_t*)audio)[2], ((uint8_t*)audio)[3],
                 ((uint8_t*)audio)[4], ((uint8_t*)audio)[5], ((uint8_t*)audio)[6], ((uint8_t*)audio)[7]);
    }
    // 1. 如果句柄为空，说明是第一次发送，需要初始化 HTTP 客户端
    if (g_baidu_asr_http_handle == NULL) {
        ESP_LOGI(TAG, "首次发送，初始化 HTTP 客户端...");
        
        char *url_str = NULL;
        int ret = asprintf(&url_str, BAIDUBCE_STT_URL, AUDIO_TOKEN);
        if (ret < 0 || url_str == NULL) {
            ESP_LOGE(TAG, "URL 内存分配失败");
            return ESP_FAIL;
        }

        esp_http_client_config_t config = {
            .url = url_str,
            .method = HTTP_METHOD_POST,
            .skip_cert_common_name_check = true,
            .buffer_size = 2048,
            .timeout_ms = 30000,
        };

        g_baidu_asr_http_handle = esp_http_client_init(&config);
        free(url_str);

        if (g_baidu_asr_http_handle == NULL) {
            ESP_LOGE(TAG, "HTTP 客户端初始化失败");
            return ESP_FAIL;
        }

        esp_http_client_set_header(g_baidu_asr_http_handle, "Content-Type", "audio/pcm;rate=16000");

        err = esp_http_client_open(g_baidu_asr_http_handle, total_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "打开 HTTP 连接失败: %s", esp_err_to_name(err));
            esp_http_client_cleanup(g_baidu_asr_http_handle);
            g_baidu_asr_http_handle = NULL;
            return err;
        }
        ESP_LOGI(TAG, "HTTP 连接已打开，准备发送总共 %d 字节的数据", total_len);
    }

    // 2. 发送音频数据
    // [修正 1] 修复指针运算错误。使用 while 循环更稳健。
    int bytes_to_send = len;
    int bytes_sent = 0;
    while (bytes_to_send > 0) {
        // 先将 audio 转为 char*，再进行字节偏移
        int written = esp_http_client_write(g_baidu_asr_http_handle, 
                                            ((const char *)audio) + bytes_sent, 
                                            bytes_to_send);
        if (written < 0) {
            ESP_LOGE(TAG, "发送音频数据失败");
            return ESP_FAIL;
        }
        bytes_sent += written;
        bytes_to_send -= written;
    }
    ESP_LOGI(TAG, "成功发送 %d 字节的音频数据", bytes_sent);
    
    return ESP_OK;
}

esp_err_t baidu_asr_recv_text(char **text)
{
    // int status_code = esp_http_client_get_status_code(g_baidu_asr_http_handle);
    // printf("Baidu ASR status_code: %d\n", status_code);
    // int content_length = esp_http_client_fetch_headers(g_baidu_asr_http_handle);
    // printf("Baidu ASR content_length: %d\n", content_length);
    // char *output_buffer = heap_caps_calloc(1, content_length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // int read_length = esp_http_client_read_response(g_baidu_asr_http_handle, output_buffer, content_length);
    // printf("Baidu ASR response: %s\n", output_buffer);
    // if (read_length > 0) {
    //     *text = baidu_stt_response_parse(output_buffer, content_length);
    // } else {
    //     ESP_LOGE(TAG, "Failed to read any data from response");

    // }

    // free(output_buffer);
    // output_buffer = NULL;
    // esp_http_client_close(g_baidu_asr_http_handle);
    // esp_http_client_cleanup(g_baidu_asr_http_handle);
    // g_baidu_asr_http_handle = NULL;

    // return ESP_OK;
    if (g_baidu_asr_http_handle == NULL) {
        ESP_LOGE(TAG, "接收错误: HTTP 客户端句柄为空，请先调用 baidu_asr_send_audio");
        return ESP_FAIL;
    }
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *text = NULL;

    esp_err_t err = ESP_FAIL;

    // [修正 2] 不使用 esp_http_client_perform，改用 fetch_headers
    // 这个函数会发送所有已缓冲的请求数据，并等待接收服务器的响应头。
    // 返回值是 Content-Length，如果服务器使用分块编码，则返回 -1。
    int content_length = esp_http_client_fetch_headers(g_baidu_asr_http_handle);
    if (content_length < 0) {
        ESP_LOGE(TAG, "获取 HTTP 响应头失败");
        goto cleanup;
    }

    // 1. 获取状态码
    int status_code = esp_http_client_get_status_code(g_baidu_asr_http_handle);
    ESP_LOGI(TAG, "HTTP 状态码: %d, Content-Length: %d", status_code, content_length);

    if (status_code != 200) {
        ESP_LOGE(TAG, "服务器返回错误状态码: %d", status_code);
        // 仍然尝试读取响应体，可能包含错误信息
    }
    
    // 2. 动态读取响应体（适用于 chunked encoding）
    int buffer_size = 1024;
    char *response_buffer = (char *)malloc(buffer_size);
    if (!response_buffer) {
        ESP_LOGE(TAG, "为响应缓冲区分配初始内存失败");
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    int total_read_len = 0;
    int read_len;
    ESP_LOGI(TAG, "开始读取服务器响应...");
    
    // 现在这个循环可以正常工作了
    while ((read_len = esp_http_client_read(g_baidu_asr_http_handle, 
                                            response_buffer + total_read_len, 
                                            buffer_size - total_read_len - 1)) > 0) {
        total_read_len += read_len;
        ESP_LOGD(TAG, "已读取 %d 字节, 总计 %d 字节", read_len, total_read_len);

        // 如果缓冲区快满了，就扩容
        if (buffer_size - total_read_len < 256) {
            buffer_size *= 2;
            char *new_buffer = (char *)realloc(response_buffer, buffer_size);
            if (!new_buffer) {
                ESP_LOGE(TAG, "重新分配响应缓冲区失败");
                free(response_buffer);
                err = ESP_ERR_NO_MEM;
                goto cleanup;
            }
            response_buffer = new_buffer;
        }
    }
    
    if (read_len < 0) {
        ESP_LOGE(TAG, "读取响应时发生错误: %s", esp_err_to_name(read_len));
        free(response_buffer);
        err = ESP_FAIL;
        goto cleanup;
    }
    
    response_buffer[total_read_len] = '\0';
    ESP_LOGI(TAG, "响应体读取完成，总长度: %d", total_read_len);
    ESP_LOGI(TAG, "收到原始响应: %s", response_buffer);

    // 3. 解析响应
    if (status_code == 200 && total_read_len > 0) {
        *text = baidu_stt_response_parse(response_buffer, total_read_len);
        if (*text) {
            ESP_LOGI(TAG, "解析结果: %s", *text);
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "解析响应 JSON 失败或结果为空");
            err = ESP_FAIL;
        }
    } else {
        err = ESP_FAIL;
    }
    
    free(response_buffer);

cleanup:
    ESP_LOGI(TAG, "关闭并清理 HTTP 客户端");
    esp_http_client_close(g_baidu_asr_http_handle);
    esp_http_client_cleanup(g_baidu_asr_http_handle);
    g_baidu_asr_http_handle = NULL;

    return err;
}

/*
* The following code sends a text input to Baidu's text-to-speech (TTS) service,
* receives an audio response, and processes it.
* 
*/
#define BAIDU_TTS_FORMAT "tex=%s&tok=%s&cuid=12345postman&ctp=1&lan=zh&spd=6&pit=5&vol=8&per=%d&aue=6&="
static esp_http_client_handle_t g_baidu_tts_http_handle = NULL;

esp_err_t baidu_tts_send_text(const char *text, uint8_t person_tone)
{
    esp_http_client_config_t config = {
        .url = "https://tsn.baidu.com/text2audio",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .buffer_size = 1460,
        .skip_cert_common_name_check = true,
    };

    // Set the headers
    g_baidu_tts_http_handle = esp_http_client_init(&config);
    esp_http_client_set_header(g_baidu_tts_http_handle, "Content-Type", "application/json");

    char *payload = NULL;
    // Create JSON payload with model, max tokens, and content

    asprintf(&payload, BAIDU_TTS_FORMAT, text, CONFIG_BAIDU_AUDIO_ACCESS_TOKEN, person_tone);
    ESP_LOGI(TAG, "payload: %s", payload);
    esp_http_client_open(g_baidu_tts_http_handle, strlen(payload));
    esp_http_client_write(g_baidu_tts_http_handle, payload, strlen(payload));
    // printf("Baidu TTS request, text to audio\n");
    free(payload);
    payload = NULL;
    return ESP_OK;
}

bool baidu_tts_recv_audio(uint8_t **data, size_t *len, size_t *total_len, bool recv)
{
    static int s_baidu_tts_audio_total_len = -1;
    if (recv) {
        if (s_baidu_tts_audio_total_len == -1) {
            s_baidu_tts_audio_total_len = esp_http_client_fetch_headers(g_baidu_tts_http_handle);
            ESP_LOGI(TAG, "s_baidu_tts_audio_total_len: %d", s_baidu_tts_audio_total_len);
            *total_len = s_baidu_tts_audio_total_len;
        }

        /* 500 ms audio */
        int audio_length = 16000 * 2 * 500 / 1000;
        // int audio_length = s_baidu_tts_audio_total_len;
        uint8_t *output_buffer = heap_caps_calloc(1, audio_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        /* Read 500 ms for audio playing */
        int read_length = esp_http_client_read(g_baidu_tts_http_handle, (char *)output_buffer, audio_length);
        // ESP_LOGI(TAG, "s_baidu_tts_audio_total_len: %d, read_length: %d", s_baidu_tts_audio_total_len, read_length);

        if (read_length > 0) {
            *len = read_length;
            *data = output_buffer;
            return true;
        } else {
            free(output_buffer);
            output_buffer = NULL;
            goto end;
        }
    } else {
        goto end;
    }

    return false;

end:
    ESP_LOGW(TAG, "End of audio response");
    *data = NULL;
    esp_http_client_close(g_baidu_tts_http_handle);
    esp_http_client_cleanup(g_baidu_tts_http_handle);
    g_baidu_tts_http_handle = NULL;
    s_baidu_tts_audio_total_len = -1;
    return false;
}
