#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "app_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

// typedef enum {
//     AUDIO_WAKENET_END = 0,
//     AUDIO_WAKENET_START,
//     AUDIO_VAD_END,
//     AUDIO_VAD_START,
//     AUDIO_VAD_WAIT,
// } audio_record_state_t;
typedef enum 
{
    ANIM_0 = 0,
    ANIM_1,
    ANIM_2,
    ANIM_TOTAL,
} Animation_List_e;

typedef struct {
    uint8_t anim_index;          // 动画索引
    lv_obj_t *anim_screen;       // 动画对应的LVGL屏幕对象
    void (*create_anim)(void);   // 动画屏幕的创建函数
} Animation_t;

esp_err_t app_animation_init();
void app_lvgl_display();
void app_animation_play(int index);
void maodie_play();

#ifdef __cplusplus
}
#endif