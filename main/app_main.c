#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sparkbot_bsp.h"
#include "app_imu.h"
#include "app_datafusion.h"
#include "app_touch.h"
#include "nvs_flash.h"

#include "esp_log.h"
#include "app_power.h"
#include "app_wifi.h"
#include "app_audio_record.h"
#include "app_animation.h"
// #include "app_AI_chat.h"
#include "app_camera.h"
#include "app_eeprom.h"
#include "driver/uart.h"
#define TAG "APP_MAIN"
#define LCD_WIDTH   240
#define LCD_HEIGHT  240

int power_voltage = 0;
uint16_t cnt = 0;

static void crop_image_to_240x240(uint16_t *src, uint16_t *dst, 
                                  int src_width, int src_height) 
{
    // 源图像尺寸：320x240
    // 目标图像尺寸：240x240
    // 裁剪策略：从宽度方向居中裁剪 240 列（从 40 到 280）
    
    int crop_x = (src_width - LCD_WIDTH) / 2;  // (320-240)/2 = 40
    int crop_y = 0;  // 高度方向不需要裁剪
    
    for (int y = 0; y < LCD_HEIGHT; y++) {
        for (int x = 0; x < LCD_WIDTH; x++) {
            // 从源图像复制像素
            dst[y * LCD_WIDTH + x] = src[(y + crop_y) * src_width + (x + crop_x)];
        }
    }
}

void memory_monitor()
{
    static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    if (1) {
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  SRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGE("MEM", "%s", buffer);
    }
}
void write_data()
{
    char cache[512] = {0}; 
    uint8_t buf = 0;
    // app_ai_chat_init();";
    char *system_data = "你是一位富有耐心、逻辑严谨的学科导师或行业顾问。你的核心特质是可靠与循循善诱，将复杂概念拆解为易于理解的步骤。";
    uint8_t buffer = 3;
    at24c08_write_bytes(0x00, &buffer, 1); //默认声音为4号
    // buffer = 2;
    if (at24c08_write_bytes(0x01, (uint8_t*)system_data, strlen(system_data)) == ESP_OK)
    {
        printf("%d\n", strlen(system_data));
        printf("write_seccess.\n");
    }
}

uint8_t app_wifi_is_connected;
void app_main(void)
{
    
    bsp_i2c_init();
    bsp_uart_init();
    app_imu_init();
    app_animation_init();
    app_touch_init();
    power_adc_init();
    app_camera_init();
    // app_sr_start();
    // app_wifi_init();
    
    while (1)
    {
        // app_imu_read();
 
        if(cnt % 500 == 0)
        {
            // printf("app_wifi:%d\n", app_wifi_is_connected);
            // power_voltage = get_power_value();
            // ESP_LOGI("POWER", "Power Voltage: %d %%", power_voltage);
            // at24c08_read_bytes(0x01, (uint8_t*)cache, strlen(system_data));
            // printf("存储的字节：%s\n", cache);
            // at24c08_read_bytes(0x00, &buf, 1);
            // printf("存储的音色：%d\n", buf);
            // ESP_LOGI("EEPROM", "read person_tone: %d", buf);
            memory_monitor();

            // uart_write_bytes(CHATBOT_UART_PORT_NUM, "1\n", 1);
        }
        if(cnt > 10000)
        {
            cnt = 0;
        }
        cnt++;
        vTaskDelay(1);
        
    }
}
