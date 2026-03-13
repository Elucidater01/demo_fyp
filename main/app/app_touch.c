#include "app_touch.h"
#include "mmap_generate_audio.h"
#include "app_audio_record.h"

#define TAG "touch"

Touch_Button_t Button = 
{
    .button_state = RELEASE,
    .Left = RELEASE,
    .Right = RELEASE,
    .Up = RELEASE,
};

int img_index = 1;

static void button_handler(touch_button_handle_t out_handle, touch_button_message_t *out_message, void *arg)
{
    (void) out_handle; //Unused
    // lv_obj_t *current_screen = lv_disp_get_scr_act(NULL);
    int button = (int)arg;
    
    if (out_message->event == TOUCH_BUTTON_EVT_ON_PRESS) 
    {
        Button.button_state = PRESS;
        ESP_LOGI("touch", "Button[%d] Press", (int)arg);
        app_sr_paly_muyu();
        switch (button) 
        {
        case 1:
            Button.Up = PRESS;
            break;
        case 2:
            Button.Left = PRESS;
            break;
        case 3:
            Button.Right = PRESS;
            break;
        default:
            break;
        }
    } 
    else if (out_message->event == TOUCH_BUTTON_EVT_ON_RELEASE) 
    {
        Button.button_state = RELEASE;
        ESP_LOGI(TAG, "Button[%d] Release", (int)arg);
        switch (button) 
        {
        case 1:
            Button.Up = RELEASE;
            break;
        case 2:
            Button.Left = RELEASE;
            break;
        case 3:
            Button.Right = RELEASE;
            break;
        default:
            break;
        }
    } 
    else if (out_message->event == TOUCH_BUTTON_EVT_ON_LONGPRESS) 
    {
        ESP_LOGI(TAG, "Button[%d] LongPress", (int)arg);
        switch (button) {
        case 1:
            Button.Up = LONGPRESS;
            break;
        case 2:
            Button.Left = LONGPRESS;
            break;
        case 3:
            Button.Right = LONGPRESS;
            break;
        default:
            break;
        }
    }
}

void app_touch_init(void)
{
    bsp_touch_button_create(button_handler);
}