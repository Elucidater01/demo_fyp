#include "app_eeprom.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

// I²C配置
// #define I2C_MASTER_SCL_IO           19      // GPIO19作为SCL
// #define I2C_MASTER_SDA_IO           18      // GPIO18作为SDA
#define I2C_MASTER_NUM              0       // I²C端口号
// #define I2C_MASTER_FREQ_HZ          100000  // I²C频率100kHz
#define I2C_MASTER_TX_BUF_DISABLE   0       // 禁用发送缓冲区
#define I2C_MASTER_RX_BUF_DISABLE   0       // 禁用接收缓冲区

// AT24C08 EEPROM配置
#define AT24C08_ADDR                0x50    // 默认I²C地址 (A0,A1,A2接地)
#define EEPROM_SIZE                 1024    // 总容量1024字节 (8Kbit)
#define PAGE_SIZE                   16      // 页大小16字节

// 日志标签
static const char *TAG = "AT24C08";


/**
 * @brief 向AT24C08写入单个字节
 * 
 * @param mem_addr 内存地址 (0-1023)
 * @param data 要写入的数据
 * @return esp_err_t 操作结果
 */
esp_err_t at24c08_write_byte(uint16_t mem_addr, uint8_t data) {
    // 检查地址有效性
    if (mem_addr >= EEPROM_SIZE) {
        ESP_LOGE(TAG, "Address out of range: %d (max %d)", mem_addr, EEPROM_SIZE - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 计算实际设备地址 (AT24C08使用分页寻址)
    uint8_t dev_addr = AT24C08_ADDR | ((mem_addr >> 8) & 0x07);
    uint8_t word_addr = mem_addr & 0xFF;  // 内存地址低8位
    
    // 准备发送数据
    uint8_t write_buf[2] = {word_addr, data};
    
    // 创建I²C命令链
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);
    i2c_master_stop(cmd);
    
    // 执行I²C传输
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 等待EEPROM内部写入完成 (最大5ms)
    vTaskDelay(pdMS_TO_TICKS(5));
    return ESP_OK;
}

/**
 * @brief 从AT24C08读取单个字节
 * 
 * @param mem_addr 内存地址 (0-1023)
 * @param data 存储读取数据的指针
 * @return esp_err_t 操作结果
 */
esp_err_t at24c08_read_byte(uint16_t mem_addr, uint8_t *data) 
{
    // 检查地址有效性
    if (mem_addr >= EEPROM_SIZE) {
        ESP_LOGE(TAG, "Address out of range: %d (max %d)", mem_addr, EEPROM_SIZE - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 计算实际设备地址
    uint8_t dev_addr = AT24C08_ADDR | ((mem_addr >> 8) & 0x07);
    uint8_t word_addr = mem_addr & 0xFF;  // 内存地址低8位
    
    // 第一步：写入要读取的内存地址
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, word_addr, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Address write failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 第二步：读取数据
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
    } 
    else 
    {
        ESP_LOGD(TAG, "Read 0x%02X from address %d", *data, mem_addr);
    }
    
    return ret;
}

/**
 * @brief 从AT24C08读取多个字节
 * 
 * @param mem_addr 起始内存地址
 * @param data 存储读取数据的缓冲区
 * @param len 要读取的字节数
 * @return esp_err_t 操作结果
 */
esp_err_t at24c08_read_bytes(uint16_t mem_addr, uint8_t *data, size_t len) 
{
    // 检查地址和长度有效性
    if (mem_addr + len > EEPROM_SIZE) 
    {
        ESP_LOGE(TAG, "Read exceeds EEPROM size");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 逐字节读取
    for (size_t i = 0; i < len; i++) 
    {
        esp_err_t ret = at24c08_read_byte(mem_addr + i, &data[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

/**
 * @brief 向AT24C08写入多个字节（页写入）
 * 
 * @param mem_addr 起始内存地址
 * @param data 要写入的数据
 * @param len 要写入的字节数
 * @return esp_err_t 操作结果
 */
esp_err_t at24c08_write_bytes(uint16_t mem_addr, uint8_t *data, size_t len) 
{
    // 检查地址和长度有效性
    if (mem_addr + len > EEPROM_SIZE) {
        ESP_LOGE(TAG, "Write exceeds EEPROM size");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 处理页边界（AT24C08页大小为16字节）
    while (len > 0) {
        // 计算当前页剩余空间
        uint16_t page_offset = mem_addr % PAGE_SIZE;
        size_t chunk_size = PAGE_SIZE - page_offset;
        if (chunk_size > len) {
            chunk_size = len;
        }
        
        // 计算实际设备地址
        uint8_t dev_addr = AT24C08_ADDR | ((mem_addr >> 8) & 0x07);
        uint8_t word_addr = mem_addr & 0xFF;
        
        // 准备发送数据（地址 + 数据）
        uint8_t write_buf[PAGE_SIZE + 1];
        write_buf[0] = word_addr;
        memcpy(&write_buf[1], data, chunk_size);
        
        // 创建I²C命令链
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, write_buf, chunk_size + 1, true);
        i2c_master_stop(cmd);
        
        // 执行I²C传输
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Page write failed: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // 等待EEPROM内部写入完成
        vTaskDelay(pdMS_TO_TICKS(5));
        
        // 更新指针和计数器
        mem_addr += chunk_size;
        data += chunk_size;
        len -= chunk_size;
    }
    
    return ESP_OK;
}

/**
 * @brief 擦除EEPROM（填充0xFF）
 */
void eeprom_erase(void) 
{
    ESP_LOGI(TAG, "Erasing EEPROM...");
    uint8_t erase_data[PAGE_SIZE];
    memset(erase_data, 0xFF, PAGE_SIZE);
    
    for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += PAGE_SIZE) {
        esp_err_t ret = at24c08_write_bytes(addr, erase_data, PAGE_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erase failed at %d", addr);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时
    }
    ESP_LOGI(TAG, "EEPROM erased successfully");
}
