#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t at24c08_write_bytes(uint16_t mem_addr, uint8_t *data, size_t len);

esp_err_t at24c08_read_bytes(uint16_t mem_addr, uint8_t *data, size_t len);
