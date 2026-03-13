#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_decoder.h"
#include "esp_lv_fs.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include <math.h> // 需包含数学库，ESP32/STM32需开启编译选项
#include "app_animation.h"

LV_IMG_DECLARE(tian);
LV_IMG_DECLARE(wanheixiu);
// LV_IMG_DECLARE(maodie);
// LV_IMG_DECLARE(shu1);
// LV_IMG_DECLARE(shu2);
// LV_IMG_DECLARE(shu3);
// LV_IMG_DECLARE(shu4);
LV_IMG_DECLARE(shu5);
// LV_IMG_DECLARE(shu6);

// 可用的GIF文件路径数组（根据您的存储方式调整路径）
// const lv_img_dsc_t *image_list[] = {
//     &maodie,
//     &shu1,
//     &shu2,
//     &shu3,
//     &shu4,
//     &shu5,
//     &shu6,
// };
// #include "mmap_generate_lottie_assets.h"
mmap_assets_handle_t asset_lottie;
esp_lv_fs_handle_t fs_drive_handle;
esp_lv_decoder_handle_t decoder_handle = NULL;
static const char *TAG = "app_animation";
lv_obj_t *image_obj = NULL;
int g_current_image_index = 0; // 当前显示的图像索引
static uint8_t current_anim_index = 0;         // 当前播放的动画索引

static void create_anim0(void);
static void create_anim1(void);
static void create_anim2(void);


static lv_obj_t *screen_list[ANIM_TOTAL] = {NULL};

Animation_t Animation_List[ANIM_TOTAL] = 
{
    {
        .anim_index = 0,
        .anim_screen = NULL,
        .create_anim = create_anim0,
    },
    {
        .anim_index = 1,
        .anim_screen = NULL,
        .create_anim = create_anim1,
    },
    {
        .anim_index = 2,
        .anim_screen = NULL,
        .create_anim = create_anim2,
    }
};

/************************* 圆周运动参数配置 *************************/
#define CENTER_X 65    // 圆心X坐标（屏幕中心）
#define CENTER_Y 80    // 圆心Y坐标
#define RADIUS 20       // 圆周运动半径
#define COMPLETE_TIME 80.0f // 完整圆周运动时间



/************************* 自定义圆周运动动画回调 *************************/
void circle_anim_cb_left(lv_obj_t * obj, lv_coord_t val) 
{
    (void)val; // 屏蔽未使用参数警告
 
    static float current_angle = 0.0f;
    // 1. 角度递增（转换为弧度：角度 * π/180）
    current_angle += 2 * M_PI / COMPLETE_TIME;
    if(current_angle >= 2 * M_PI) { // 超过360度重置，避免数值溢出
        current_angle = 0.0f;
    }

    // 2. 计算正方形中心的圆周坐标
    float center_x = 70 + RADIUS * cos(current_angle);
    float center_y = CENTER_Y + RADIUS * sin(current_angle);

    float obj_x = center_x - 40 / 2;
    float obj_y = center_y - 30 / 2;

    // 4. 更新正方形位置
    lv_obj_set_pos(obj, (lv_coord_t)obj_x, (lv_coord_t)obj_y);
}

void circle_anim_cb_right(lv_obj_t * obj, lv_coord_t val) 
{
    (void)val; // 屏蔽未使用参数警告
   
    static float current_angle = M_PI;
    // 1. 角度递增（转换为弧度：角度 * π/180）
    current_angle = current_angle + 2 * M_PI / COMPLETE_TIME;
    if(current_angle >= 2 * M_PI) { // 超过360度重置，避免数值溢出
        current_angle = 0.0f;
    }

    // 2. 计算正方形中心的圆周坐标
    float center_x = 155 + RADIUS * cos(current_angle);
    float center_y = CENTER_Y + RADIUS * sin(current_angle);

    float obj_x = center_x - 40 / 2;
    float obj_y = center_y - 30 / 2;

    // 4. 更新正方形位置
    lv_obj_set_pos(obj, (lv_coord_t)obj_x, (lv_coord_t)obj_y);
}

// 动画0：眨眼动画（双眼长方形）
static void create_anim0(void) 
{
    // 创建眨眼动画组件
    lv_obj_t *eye1 = lv_obj_create(screen_list[0]);
    lv_obj_set_size(eye1, 40, 30);
    lv_obj_set_pos(eye1, 55, 80);
    lv_obj_set_style_bg_color(eye1, lv_color_hex(0x0066CC), LV_PART_MAIN);

    lv_obj_t *eye2 = lv_obj_create(screen_list[0]);
    lv_obj_set_size(eye2, 40, 30);
    lv_obj_set_pos(eye2, 145, 80);
    lv_obj_set_style_bg_color(eye2, lv_color_hex(0x0066CC), LV_PART_MAIN);

    // 配置眨眼动画
    lv_anim_t anim_height;
    lv_anim_init(&anim_height);
    lv_anim_set_var(&anim_height, eye1);
    lv_anim_set_exec_cb(&anim_height, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_time(&anim_height, 200);
    lv_anim_set_values(&anim_height, 30, 5);    
    lv_anim_set_path_cb(&anim_height, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_height, 1000);    
    lv_anim_set_playback_delay(&anim_height, 0);    
    lv_anim_set_playback_time(&anim_height, 200);    
    lv_anim_set_repeat_delay(&anim_height, 5000);  
    lv_anim_set_repeat_count(&anim_height, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_height);

    // 2. 创建「位置y变化」的anim
    lv_anim_t anim_y;
    lv_anim_init(&anim_y);
    lv_anim_set_var(&anim_y, eye1);
    lv_anim_set_exec_cb(&anim_y, (lv_anim_exec_xcb_t)lv_obj_set_y); // 控制Y坐标
    lv_anim_set_values(&anim_y, 80, 90); // Y从80→90
    lv_anim_set_time(&anim_y, 200);
    lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_y, 1000);    
    lv_anim_set_playback_delay(&anim_y, 0);    
    lv_anim_set_playback_time(&anim_y, 200);    
    lv_anim_set_repeat_delay(&anim_y, 5000);  
    lv_anim_set_repeat_count(&anim_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_y);

    lv_anim_t anim_x;
    lv_anim_init(&anim_x);
    lv_anim_set_var(&anim_x, eye1);
    lv_anim_set_exec_cb(&anim_x, (lv_anim_exec_xcb_t)lv_obj_set_x); // 控制Y坐标
    lv_anim_set_values(&anim_x, 55, 75); // Y从80→90
    lv_anim_set_time(&anim_x, 500);
    lv_anim_set_path_cb(&anim_x, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_x, 0);    
    lv_anim_set_playback_delay(&anim_x, 500);    
    lv_anim_set_playback_time(&anim_x, 500);    
    lv_anim_set_repeat_delay(&anim_x, 2000);  
    lv_anim_set_repeat_count(&anim_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_x);

    lv_anim_t anim_h2;
    lv_anim_init(&anim_h2);
    lv_anim_set_var(&anim_h2, eye2);
    lv_anim_set_exec_cb(&anim_h2, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_time(&anim_h2, 200);
    lv_anim_set_values(&anim_h2, 30, 5);    
    lv_anim_set_path_cb(&anim_h2, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_h2, 1000);    
    lv_anim_set_playback_delay(&anim_h2, 0);    
    lv_anim_set_playback_time(&anim_h2, 200);    
    lv_anim_set_repeat_delay(&anim_h2, 5000);  
    lv_anim_set_repeat_count(&anim_h2, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_h2);

    lv_anim_t anim_y2;
    lv_anim_init(&anim_y2);
    lv_anim_set_var(&anim_y2, eye2);
    lv_anim_set_exec_cb(&anim_y2, (lv_anim_exec_xcb_t)lv_obj_set_y); // 控制Y坐标
    lv_anim_set_values(&anim_y2, 80, 90); // Y从80→90
    lv_anim_set_time(&anim_y2, 200);
    lv_anim_set_path_cb(&anim_y2, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_y2, 1000);    
    lv_anim_set_playback_delay(&anim_y2, 0);    
    lv_anim_set_playback_time(&anim_y2, 200);    
    lv_anim_set_repeat_delay(&anim_y2, 5000);  
    lv_anim_set_repeat_count(&anim_y2, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_y2);

    lv_anim_t anim_x2;
    lv_anim_init(&anim_x2);
    lv_anim_set_var(&anim_x2, eye2);
    lv_anim_set_exec_cb(&anim_x2, (lv_anim_exec_xcb_t)lv_obj_set_x); // 控制Y坐标
    lv_anim_set_values(&anim_x2, 145, 165); // Y从80→90
    lv_anim_set_time(&anim_x2, 500);
    lv_anim_set_path_cb(&anim_x2, lv_anim_path_ease_in_out);    
    lv_anim_set_delay(&anim_x2, 0);    
    lv_anim_set_playback_delay(&anim_x2, 500);    
    lv_anim_set_playback_time(&anim_x2, 500);    
    lv_anim_set_repeat_delay(&anim_x2, 2000);  
    lv_anim_set_repeat_count(&anim_x2, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_x2);

    // // 创建眨眼动画组件
    // lv_obj_t *eye1 = lv_obj_create(screen_list[0]);
    // lv_obj_set_size(eye1, 40, 30);
    // lv_obj_set_pos(eye1, 55, 160);
    // lv_obj_set_style_bg_color(eye1, lv_color_hex(0x0066CC), LV_PART_MAIN);

    // lv_obj_t *eye2 = lv_obj_create(screen_list[0]);
    // lv_obj_set_size(eye2, 40, 30);
    // lv_obj_set_pos(eye2, 145, 160);
    // lv_obj_set_style_bg_color(eye2, lv_color_hex(0x0066CC), LV_PART_MAIN);

    // // 配置眨眼动画
    // lv_anim_t anim_height;
    // lv_anim_init(&anim_height);
    // lv_anim_set_var(&anim_height, eye1);
    // lv_anim_set_exec_cb(&anim_height, (lv_anim_exec_xcb_t)lv_obj_set_height);
    // lv_anim_set_time(&anim_height, 200);
    // lv_anim_set_values(&anim_height, 30, 5);    
    // lv_anim_set_path_cb(&anim_height, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_height, 1000);    
    // lv_anim_set_playback_delay(&anim_height, 0);    
    // lv_anim_set_playback_time(&anim_height, 200);    
    // lv_anim_set_repeat_delay(&anim_height, 5000);  
    // lv_anim_set_repeat_count(&anim_height, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_height);

    // // 2. 创建「位置y变化」的anim
    // lv_anim_t anim_y;
    // lv_anim_init(&anim_y);
    // lv_anim_set_var(&anim_y, eye1);
    // lv_anim_set_exec_cb(&anim_y, (lv_anim_exec_xcb_t)lv_obj_set_y); // 控制Y坐标
    // lv_anim_set_values(&anim_y, 150, 160); // Y从160→170
    // lv_anim_set_time(&anim_y, 200);
    // lv_anim_set_path_cb(&anim_y, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_y, 1000);    
    // lv_anim_set_playback_delay(&anim_y, 0);    
    // lv_anim_set_playback_time(&anim_y, 200);    
    // lv_anim_set_repeat_delay(&anim_y, 5000);  
    // lv_anim_set_repeat_count(&anim_y, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_y);

    // lv_anim_t anim_x;
    // lv_anim_init(&anim_x);
    // lv_anim_set_var(&anim_x, eye1);
    // lv_anim_set_exec_cb(&anim_x, (lv_anim_exec_xcb_t)lv_obj_set_x); // 控制Y坐标
    // lv_anim_set_values(&anim_x, 55, 75); // Y从80→90
    // lv_anim_set_time(&anim_x, 500);
    // lv_anim_set_path_cb(&anim_x, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_x, 0);    
    // lv_anim_set_playback_delay(&anim_x, 500);    
    // lv_anim_set_playback_time(&anim_x, 500);    
    // lv_anim_set_repeat_delay(&anim_x, 2000);  
    // lv_anim_set_repeat_count(&anim_x, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_x);

    // lv_anim_t anim_h2;
    // lv_anim_init(&anim_h2);
    // lv_anim_set_var(&anim_h2, eye2);
    // lv_anim_set_exec_cb(&anim_h2, (lv_anim_exec_xcb_t)lv_obj_set_height);
    // lv_anim_set_time(&anim_h2, 200);
    // lv_anim_set_values(&anim_h2, 30, 5);    
    // lv_anim_set_path_cb(&anim_h2, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_h2, 1000);    
    // lv_anim_set_playback_delay(&anim_h2, 0);    
    // lv_anim_set_playback_time(&anim_h2, 200);    
    // lv_anim_set_repeat_delay(&anim_h2, 5000);  
    // lv_anim_set_repeat_count(&anim_h2, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_h2);

    // lv_anim_t anim_y2;
    // lv_anim_init(&anim_y2);
    // lv_anim_set_var(&anim_y2, eye2);
    // lv_anim_set_exec_cb(&anim_y2, (lv_anim_exec_xcb_t)lv_obj_set_y); // 控制Y坐标
    // lv_anim_set_values(&anim_y2, 150, 160); // Y从160→170
    // lv_anim_set_time(&anim_y2, 200);
    // lv_anim_set_path_cb(&anim_y2, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_y2, 1000);    
    // lv_anim_set_playback_delay(&anim_y2, 0);    
    // lv_anim_set_playback_time(&anim_y2, 200);    
    // lv_anim_set_repeat_delay(&anim_y2, 5000);  
    // lv_anim_set_repeat_count(&anim_y2, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_y2);

    // lv_anim_t anim_x2;
    // lv_anim_init(&anim_x2);
    // lv_anim_set_var(&anim_x2, eye2);
    // lv_anim_set_exec_cb(&anim_x2, (lv_anim_exec_xcb_t)lv_obj_set_x); // 控制Y坐标
    // lv_anim_set_values(&anim_x2, 145, 165); // Y从80→90
    // lv_anim_set_time(&anim_x2, 500);
    // lv_anim_set_path_cb(&anim_x2, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_x2, 0);    
    // lv_anim_set_playback_delay(&anim_x2, 500);    
    // lv_anim_set_playback_time(&anim_x2, 500);    
    // lv_anim_set_repeat_delay(&anim_x2, 2000);  
    // lv_anim_set_repeat_count(&anim_x2, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_x2);
}

// 动画1：左右移动动画（单个长方形）
static void create_anim1(void) 
{
    // lv_obj_t *eye1 = lv_obj_create(screen_list[1]);
    // lv_obj_set_size(eye1, 40, 30);
    // lv_obj_set_pos(eye1, 55, 80);
    // lv_obj_set_style_bg_color(eye1, lv_color_hex(0x0066CC), LV_PART_MAIN);

    // lv_obj_t *eye2 = lv_obj_create(screen_list[1]);
    // lv_obj_set_size(eye2, 40, 30);
    // lv_obj_set_pos(eye2, 145, 80);
    // lv_obj_set_style_bg_color(eye2, lv_color_hex(0x0066CC), LV_PART_MAIN);

    // /* 画圆 */
    // lv_anim_t anim_left;
    // lv_anim_init(&anim_left);
    // lv_anim_set_var(&anim_left, eye1);
    // lv_anim_set_exec_cb(&anim_left, (lv_anim_exec_xcb_t)circle_anim_cb_left);
    // lv_anim_set_time(&anim_left, COMPLETE_TIME);
    // lv_anim_set_repeat_count(&anim_left, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_left);

    // lv_anim_t anim_right;
    // lv_anim_init(&anim_right);
    // lv_anim_set_var(&anim_right, eye2);
    // lv_anim_set_exec_cb(&anim_right, (lv_anim_exec_xcb_t)circle_anim_cb_right);
    // lv_anim_set_time(&anim_right, COMPLETE_TIME);
    // lv_anim_set_repeat_count(&anim_right, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_right);
    /*****************************************************************************/
    // /* 配置上下移动动画 */
    // lv_anim_t anim_left_y;
    // lv_anim_init(&anim_left_y);
    // lv_anim_set_var(&anim_left_y, eye1);
    // lv_anim_set_exec_cb(&anim_left_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    // lv_anim_set_values(&anim_left_y, 80, 100); // Y从80→90
    // lv_anim_set_time(&anim_left_y, 400);
    // lv_anim_set_path_cb(&anim_left_y, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_left_y, 0);
    // lv_anim_set_playback_delay(&anim_left_y, 400);    
    // lv_anim_set_playback_time(&anim_left_y, 400);    
    // lv_anim_set_repeat_delay(&anim_left_y, 400);
    // lv_anim_set_repeat_count(&anim_left_y, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_left_y);

    // /* 配置左右移动动画 */
    // lv_anim_t anim_left_x;
    // lv_anim_init(&anim_left_x);
    // lv_anim_set_var(&anim_left_x, eye1);
    // lv_anim_set_exec_cb(&anim_left_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    // lv_anim_set_values(&anim_left_x, 55, 35); // Y从80→90
    // lv_anim_set_time(&anim_left_x, 400);
    // lv_anim_set_path_cb(&anim_left_x, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_left_x, 400);
    // lv_anim_set_playback_delay(&anim_left_x, 400);    
    // lv_anim_set_playback_time(&anim_left_x, 400);    
    // lv_anim_set_repeat_delay(&anim_left_x, 400);
    // lv_anim_set_repeat_count(&anim_left_x, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_left_x);

    // /* 配置上下移动动画 */
    // lv_anim_t anim_right_y;
    // lv_anim_init(&anim_right_y);
    // lv_anim_set_var(&anim_right_y, eye2);
    // lv_anim_set_exec_cb(&anim_right_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    // lv_anim_set_values(&anim_right_y, 80, 60); // Y从80→90
    // lv_anim_set_time(&anim_right_y, 400);
    // lv_anim_set_path_cb(&anim_right_y, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_right_y, 0);
    // lv_anim_set_playback_delay(&anim_right_y, 400);    
    // lv_anim_set_playback_time(&anim_right_y, 400);    
    // lv_anim_set_repeat_delay(&anim_right_y, 400);
    // lv_anim_set_repeat_count(&anim_right_y, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_right_y);

    // /* 配置左右移动动画 */
    // lv_anim_t anim_right_x;
    // lv_anim_init(&anim_right_x);
    // lv_anim_set_var(&anim_right_x, eye2);
    // lv_anim_set_exec_cb(&anim_right_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    // lv_anim_set_values(&anim_right_x, 145, 165); // Y从80→90
    // lv_anim_set_time(&anim_right_x, 400);
    // lv_anim_set_path_cb(&anim_right_x, lv_anim_path_ease_in_out);    
    // lv_anim_set_delay(&anim_right_x, 400);
    // lv_anim_set_playback_delay(&anim_right_x, 400);    
    // lv_anim_set_playback_time(&anim_right_x, 400);    
    // lv_anim_set_repeat_delay(&anim_right_x, 400);
    // lv_anim_set_repeat_count(&anim_right_x, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_start(&anim_right_x);
}

// 动画2：缩放动画（单个长方形）
static void create_anim2(void) 
{
    /* 创建一个画布对象 */
    // lv_obj_t *canvas = lv_canvas_create(screen_list[2]);

    /*  绘制 > 表情*/
    lv_obj_t *line1 = lv_line_create(screen_list[2]);
    static lv_point_t points1[3] = {{65, 60}, {105, 80}, {65, 100}};
    lv_line_set_points(line1, points1, 3);
    lv_obj_set_style_line_width(line1, 20, LV_PART_MAIN);
    lv_obj_set_style_line_color(line1, lv_color_hex(0x0066CC), LV_PART_MAIN);
    // lv_obj_set_style_line_rounded(line1, true, LV_PART_MAIN);
    lv_obj_set_style_line_width(line1, 5, LV_PART_MAIN);
    
    lv_obj_t *line2 = lv_line_create(screen_list[2]);
    static lv_point_t points2[3] = {{185, 60}, {145, 80}, {185, 100}};
    lv_line_set_points(line2, points2, 3);
    lv_obj_set_style_line_width(line2, 20, LV_PART_MAIN);
    lv_obj_set_style_line_color(line2, lv_color_hex(0x0066CC), LV_PART_MAIN);
    // lv_obj_set_style_line_rounded(line2, true, LV_PART_MAIN);
    lv_obj_set_style_line_width(line2, 5, LV_PART_MAIN);

    /* 绘制 < 表情 */
    // lv_obj_t *line3 = lv_line_create(screen_list[2]);
    // lv_point_t points3[2] = {{180, 60}, {160, 80}};
    // lv_line_set_points(line3, points3, 2);
    // lv_obj_set_style_line_width(line3, 5, LV_PART_MAIN);
    // lv_obj_set_style_line_color(line3, lv_color_hex(0xFF0000), LV_PART_MAIN);
    // lv_obj_set_style_line_rounded(line3, true, LV_PART_MAIN);
    // lv_obj_set_style_line_width(line3, 5, LV_PART_MAIN);

    // lv_obj_t *line4 = lv_line_create(screen_list[2]);
    // lv_point_t points4[2] = {{180, 100}, {160, 80}};
    // lv_line_set_points(line4, points4, 2);
    // lv_obj_set_style_line_width(line4, 5, LV_PART_MAIN);
    // lv_obj_set_style_line_color(line4, lv_color_hex(0xFF0000), LV_PART_MAIN);
    // lv_obj_set_style_line_rounded(line4, true, LV_PART_MAIN);
    // lv_obj_set_style_line_width(line4, 5, LV_PART_MAIN);
}

void app_lvgl_display()
{
    // 2. 创建一个 对象
    // image_obj = lv_gif_create(lv_scr_act()); 
    // image_obj = lv_img_create(lv_scr_act());
    
    // 3. 设置 GIF 图像的源为之前声明的数组
    // lv_gif_set_src(image_obj, &tian);
    // lv_img_set_src(image_obj, &shu5);
    
    // 4. 将 GIF 对象对齐到屏幕中心（可选）
    // lv_obj_align(image_obj, LV_ALIGN_CENTER, 0, 0);
    // /* 2. 创建蓝色矩形（右眼） */
    // lv_obj_t * rect_blue = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(rect_blue, 80, 60);
    
    // lv_obj_remove_style_all(rect_blue);
    // lv_obj_set_style_bg_color(rect_blue, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    // lv_obj_set_style_bg_opa(rect_blue, LV_OPA_COVER, LV_PART_MAIN);
    // lv_obj_set_style_radius(rect_blue, 10, LV_PART_MAIN);

    // /* 将蓝色矩形定位在屏幕右侧 */
    // lv_obj_align(rect_blue, LV_ALIGN_RIGHT_MID, -50, 0);
}

void app_animation_play(int index)
{
    if(index >= ANIM_TOTAL) index = 0;
    if(index == current_anim_index) return;

    // // 销毁当前屏幕（释放内存）
    // if(Animation_List[current_anim_index].anim_screen) 
    // {
    //     lv_obj_del(Animation_List[current_anim_index].anim_screen);
    // }
    
    // 加载屏幕
    lv_scr_load(Animation_List[index].anim_screen);
    // 动态创建目标屏幕
    // Animation_List[index].create_anim();
    current_anim_index = index;
}

void screen_list_init()
{
    for(int i = 0; i < ANIM_TOTAL; i++)
    {
        screen_list[i] = lv_obj_create(NULL);
        Animation_List[i].anim_screen = screen_list[i];
        Animation_List[i].create_anim();
    }
    lv_scr_load(screen_list[0]);
}

void lvgl_task(void *pvParameters)
{
    uint16_t cnt = 0;
    screen_list_init();
    
    while (1)
    {
        if(Button.Left == PRESS)
        {
            app_animation_play(0);
        }
        else if(Button.Right == PRESS)
        {
            // app_animation_play(1);
        }
        else if(Button.Up == PRESS)
        {
            // app_animation_play(2);
        }

        if(cnt++ >= 10)
        {
            cnt = 0;
            lv_timer_handler();
        }
        vTaskDelay(1);
    }
}

esp_err_t app_animation_init()
{
    /* Initialize display and LVGL */
    bsp_display_cfg_t custom_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .trans_size = BSP_LCD_H_RES * 10, // in SRAM, DMA-capable
        .double_buffer = 0,
        .flags = 
        {
            .buff_dma = false,
            .buff_spiram = true,
        }
    };
    custom_cfg.lvgl_port_cfg.task_stack = 1024 * 30;
    custom_cfg.lvgl_port_cfg.task_affinity = 1;
    bsp_display_start_with_config(&custom_cfg);

    /* Turn on display backlight */
    bsp_display_backlight_on();

    // app_mount_mmap_fs();

    // ESP_ERROR_CHECK(lv_fs_add());

    // ESP_ERROR_CHECK(esp_lv_decoder_init(&decoder_handle));

    /* Create a task to handle LVGL tasks */
    // xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 1024 * 10, NULL, 10, NULL, 1);

    /* Add and show objects on display */
    // app_lvgl_display();

    return ESP_OK;
}
