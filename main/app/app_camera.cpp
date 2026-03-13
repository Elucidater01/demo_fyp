#include "app_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sparkbot_bsp.h"
// #include "mbedtls/base64.h"
#include "esp_log.h"
#include "human_face_detect.hpp"
#include "driver/uart.h"

// #include "dl_image.hpp"

#define TAG "APP_CAMERA"

uint8_t send_array[] = {0x00, 0x00};

// 人脸检测器实例
HumanFaceDetect *detector = new HumanFaceDetect();

uint16_t *frame_buffer = NULL;

// // 将 RGB565 图像转换为 Base64 字符串
// char* convert_rgb565_to_base64(const uint8_t *rgb565_data) {
//     // 步骤1: 转换为 RGB888 (可选，但更通用)
//     int pixel_count = 240 * 240;
//     uint8_t *rgb888_data = malloc(pixel_count * 3); // RGB888 每个像素3字节
    
//     if (!rgb888_data) {
//         ESP_LOGE(TAG, "Failed to allocate memory for RGB888 conversion");
//         return NULL;
//     }
    
//     // rgb565_to_rgb888(rgb565_data, rgb888_data, pixel_count);
    
//     // 步骤2: 计算 Base64 编码后的大小
//     size_t output_len = ((pixel_count * 3) + 2) / 3 * 4 + 1; // +1 for null terminator
//     char *base64_str = malloc(output_len);
    
//     if (!base64_str) {
//         ESP_LOGE(TAG, "Failed to allocate memory for Base64 string");
//         free(rgb888_data);
//         return NULL;
//     }
    
//     // 步骤3: 执行 Base64 编码
//     size_t encoded_len;
//     int ret = mbedtls_base64_encode((unsigned char *)base64_str, output_len, 
//                                    &encoded_len, rgb888_data, pixel_count * 3);
    
//     free(rgb888_data); // 释放临时 RGB888 数据
    
//     if (ret != 0) {
//         ESP_LOGE(TAG, "Base64 encoding failed: %d", ret);
//         free(base64_str);
//         return NULL;
//     }
    
//     // 确保字符串以 null 结尾
//     base64_str[encoded_len] = '\0';
//     return base64_str;
// }

void camera_task(void *pvParameters)
{
    char *base64_str = NULL;

    while (1)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) 
        {
            printf("Camera Capture Failed\n");
            continue;
        }
        else
        {
            // 处理捕获的图像数据
            if (fb->len > (240 * 240 * sizeof(uint16_t))) 
            {
                printf("错误：图像数据大小(%d)超过显示缓冲区容量！\n", fb->len);
                esp_camera_fb_return(fb);
                // 处理错误，例如跳过本帧或调整分辨率
                continue;
            }
            memcpy(frame_buffer, fb->buf, fb->len);
            display_image(frame_buffer);

            dl::image::img_t input_img;
            input_img.data = (uint8_t*)fb->buf;
            input_img.width = fb->width;
            input_img.height = fb->height;
            input_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565;

            // // 创建候选框列表
            // std::list<dl::detect::result_t> candidates;
            
            // // 正确初始化候选框
            // dl::detect::result_t candidate;
            // candidate.box.push_back(0);     // x1
            // candidate.box.push_back(0);     // y1
            // candidate.box.push_back(input_img.width);  // x2
            // candidate.box.push_back(input_img.height); // y2
            // candidate.score = 1.0f;
            // candidates.push_back(candidate);

            // 运行人脸检测
            std::list<dl::detect::result_t> detection_results = detector->run(input_img);
            if (!detector) 
            {
                printf("错误: 检测器未初始化\n");
                continue;
            }
    
            // 检查检测结果
            size_t face_count = detection_results.size();
            printf("检测到 %lu 个人脸\n", (unsigned long)face_count);
    
            if (detection_results.empty()) 
            {
                printf("未检测到人脸，可能原因:\n");
                printf("- 图像太暗/太亮\n");
                printf("- 没有人脸在画面中\n");
                printf("- 摄像头距离太远\n");
                printf("- 检测器阈值设置过高\n");
            }
            else
            {
                int face_index = 0;
                for (auto &face : detection_results) 
                {
                    face_index++;
                    if (face.box.size() < 4) 
                    {
                        printf("人脸框坐标数据不完整\n");
                        continue;
                    }
                    // 获取人脸边界框坐标 [x1, y1, x2, y2]
                    int x1 = face.box[0];
                    int y1 = face.box[1]; 
                    int x2 = face.box[2];
                    int y2 = face.box[3];
                    float confidence = face.score;
                    
                    // 计算中心点坐标（用于追踪）
                    int center_x = (x1 + x2) / 2;
                    int center_y = (y1 + y2) / 2;
                    int width = x2 - x1;
                    int height = y2 - y1;
                    printf("人脸 %d: 位置(%d,%d) 大小(%dx%d) 置信度: %.2f\n", 
                    center_x, center_y, width, height, confidence);

                    if (center_x > 120)
                    {
                        send_array[0] = 0x01;
                    }
                    else if (center_x < 120)
                    {
                        send_array[0] = 0x02;
                    }
                    if (center_y > 120)
                    {
                        send_array[1] = 0x01;
                    }
                    else if (center_y < 120)
                    {
                        send_array[1] = 0x02;
                    }
                    uart_write_bytes((uart_port_t)CHATBOT_UART_PORT_NUM, (const char*)send_array, sizeof(send_array));
                    // uart_write_bytes(CHATBOT_UART_PORT_NUM, "hello", 5);
                }
            }
            vTaskDelay(10);
            // 释放帧缓冲区
            esp_camera_fb_return(fb);
        }

        vTaskDelay(50); // 每秒捕获一次图像
    }
}
// base64_str = convert_rgb565_to_base64(frame_buffer);
            // if (base64_str) 
            // {
            //     ESP_LOGI(TAG, "Base64 encoded string (first 100 chars): %.100s", base64_str);
        
            //     // 使用完成后释放内存
            //     free(base64_str);
            // }
void app_camera_init()
{
     /* 摄像头初始化 */
    const camera_config_t camera_config = BSP_CAMERA_DEFAULT_CONFIG;
   
    if (esp_camera_init(&camera_config) != ESP_OK) 
    {
        printf("Camera Init Failed");
        return;
    }
    else
    {
        printf("Camera Init Success\n");
    }

    // 获取摄像头传感器对象
    sensor_t *s = esp_camera_sensor_get();
    framesize_t frame_size = s->status.framesize;

    // 分配缓冲区 - 根据实际帧大小
    size_t frame_size_bytes = 240 * 240 * 2;  // RGB565 = 2字节/像素
    // frame_buffer = heap_caps_malloc(frame_size_bytes, MALLOC_CAP_SPIRAM);
    // frame_buffer = (uint16_t*)heap_caps_malloc(frame_size_bytes, MALLOC_CAP_SPIRAM);
    frame_buffer = (uint16_t*)heap_caps_malloc(240 * 240 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCore(camera_task, "camera_task", 4096, NULL, 5, NULL, 1);
}
