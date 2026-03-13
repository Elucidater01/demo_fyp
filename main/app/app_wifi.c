
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "app_wifi.h"
// 在 app_wifi.c 文件顶部添加
#include "freertos/event_groups.h"
// #include "app_audio_record.h"

// 定义事件位（使用 24 位有效位）
#define WIFI_CONNECTED_BIT    BIT0      // 位0：WiFi连接成功
#define WIFI_DISCONNECTED_BIT BIT1      // 位1：WiFi断开连接

// 在 app_wifi.c 文件顶部添加
#define DEFAULT_SCAN_LIST_SIZE 10  // 最大扫描AP数量

// WiFi 配置参数
#define TARGET_SSID "HONOR_X50_GT"        // 替换为目标WiFi名称TP-Link_731C
#define TARGET_PASSWORD "43116978zds" // 替换为目标WiFi密码66650232
#define MAX_RETRY 3               // 最大重试次数

// 日志标签
static const char *TAG = "WIFI_SCAN_CONNECT";

// 事件组用于同步连接状态
static EventGroupHandle_t wifi_event_group;
static uint8_t is_wifi_connected = 0;

uint8_t app_get_wifi_connected_state()
{
    if (is_wifi_connected)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 事件处理回调函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) 
    {
        if (event_id == WIFI_EVENT_STA_START) 
        {
            ESP_LOGI(TAG, "WiFi启动,开始扫描...");
            esp_wifi_scan_start(NULL, true); // 启动全信道扫描
        } 
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) 
        {
            ESP_LOGW(TAG, "WiFi断开,尝试重新扫描连接...");
            esp_wifi_scan_start(NULL, true);
        }
    } 
    else if (event_base == IP_EVENT) 
    {
        if (event_id == IP_EVENT_STA_GOT_IP) 
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "成功获取IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            is_wifi_connected = 1;
        }
    }
}

// 扫描并连接目标WiFi
void wifi_scan_connect() 
{
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    bool found_target = false;
    uint8_t is_connected = 0;

    for (uint8_t attempt = 0; attempt < MAX_RETRY; attempt++)
    {
        // 执行扫描（阻塞模式）
        ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

        ESP_LOGI(TAG, "扫描到 %d 个AP:", ap_count);
        for (int i = 0; i < ap_count; i++) 
        {
            ESP_LOGI(TAG, "SSID: %-20s RSSI: %d Channel: %d",
                    ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);

            // 匹配目标SSID
            if (strcmp((char*)ap_info[i].ssid, TARGET_SSID) == 0) {
                found_target = true;
                ESP_LOGI(TAG, "找到目标AP: %s", TARGET_SSID);

                // 配置WiFi连接参数
                wifi_config_t wifi_config = {
                    .sta = {
                        .ssid = TARGET_SSID,
                        .password = TARGET_PASSWORD,
                        .scan_method = WIFI_ALL_CHANNEL_SCAN,
                        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                        .bssid_set = false
                    }
                };

                // 断开当前连接并应用新配置
                esp_wifi_disconnect();
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                esp_wifi_connect();
                is_connected = 1;
                ESP_LOGI(TAG,"正在连接到 %s ...\n", TARGET_SSID);

                break;
            }
        }

        if (!found_target) 
        {
            ESP_LOGE(TAG, "未找到目标AP: %s", TARGET_SSID);
        }
        if (is_connected) 
        {
            ESP_LOGI(TAG,"连接成功!\n");
            break; // 成功连接，退出重试循环
        } 
        else 
        {
            ESP_LOGI(TAG, "重试连接... (%d/%d)", attempt + 1, MAX_RETRY);
        }
        vTaskDelay(5000); // 等待5秒后重试
    }
}

// 初始化WiFi
void app_wifi_init() {
    // 创建事件组
    wifi_event_group = xEventGroupCreate();

    // 初始化NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // 初始化网络接口和事件循环
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    // 配置WiFi参数
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理器
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    // 启动WiFi
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi初始化完成");
    wifi_scan_connect();
}

