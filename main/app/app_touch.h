#ifndef _APP_TOUCH_H_
#define _APP_TOUCH_H_

#include "esp_sparkbot_bsp.h"
#include "app_animation.h"
#include "esp_log.h"

typedef enum
{
    PRESS,
    RELEASE,
    LONGPRESS,
} Buttion_State_e;

typedef struct
{
    Buttion_State_e button_state;
    Buttion_State_e Left;
    Buttion_State_e Right;
    Buttion_State_e Up;
} Touch_Button_t;

extern Touch_Button_t Button;

void app_touch_init(void);

#endif