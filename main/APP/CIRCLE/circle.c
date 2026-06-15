#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

#include "circle.h"
#include "lcd.h"
#include "imu_service.h"
#include "xl9555.h"

#include "qma6100p_task.h"

static const char *TAG = "CIRCLE";


/* ===== 基本参数 ===== */
#define RADIUS      25

/* 方向调节 */
#define INVERT_X  0
#define INVERT_Y  0

#define ACC_SCALE     0.30f    
#define DAMPING       0.99f    
#define MAX_SPEED     100.0f    

#define CENTER_FORCE  0.02f    // 越大回中越快

#define DEAD_ZONE     1.0f
#define STOP_EPS      0.05f    // 速度很小时直接归零

static uint16_t center_x = 0;
static uint16_t center_y = 0;
static uint16_t last_cx = 0, last_cy = 0;
static bool first_run = true;

/* ===== 物理状态 ===== */
static float pos_x = 0;
static float pos_y = 0;
static float vel_x = 0;
static float vel_y = 0;

/* ===== 死区函数 ===== */
static float apply_deadzone(float v)
{
    if (fabsf(v) < DEAD_ZONE) return 0;
    return v;
}

void circle_update(void)
{
    if (panel_handle == NULL) return;

    // 非阻塞尝试获取 LCD 互斥锁，若被占用则跳过本次更新
    if (!lcd_trylock()) {
        return;
    }

    float roll  = imu_service_get_roll();
    float pitch = imu_service_get_pitch();

    /* ===== 坐标处理 ===== */
    float x = pitch;
    float y = roll;

    if (INVERT_X) x = -x;
    if (INVERT_Y) y = -y;

    x = apply_deadzone(x);
    y = apply_deadzone(y);

    /* ===== 限制角度 ===== */
    if (x > 90) x = 90;
    if (x < -90) x = -90;
    if (y > 90) y = 90;
    if (y < -90) y = -90;

    /* ===== 屏幕范围 ===== */
    float max_x = (spilcddev.width  / 2 - RADIUS);
    float max_y = (spilcddev.height / 2 - RADIUS);

    /* ===== 角度 → 加速度 ===== */
    float acc_x = x * ACC_SCALE;
    float acc_y = y * ACC_SCALE;

    /* ===== 回中力 ===== */
    acc_x += -pos_x * CENTER_FORCE;
    acc_y += -pos_y * CENTER_FORCE;

    /* ===== 更新速度 ===== */
    vel_x += acc_x;
    vel_y += acc_y;

    /* ===== 限速 ===== */
    if (vel_x > MAX_SPEED) vel_x = MAX_SPEED;
    if (vel_x < -MAX_SPEED) vel_x = -MAX_SPEED;
    if (vel_y > MAX_SPEED) vel_y = MAX_SPEED;
    if (vel_y < -MAX_SPEED) vel_y = -MAX_SPEED;

    /* ===== 阻尼 ===== */
    vel_x *= DAMPING;
    vel_y *= DAMPING;

    /* ===== 微小速度归零（防抖） ===== */
    if (fabsf(vel_x) < STOP_EPS) vel_x = 0;
    if (fabsf(vel_y) < STOP_EPS) vel_y = 0;

    /* ===== 更新位置 ===== */
    pos_x += vel_x;
    pos_y += vel_y;

    /* ===== 边界 + 反弹 ===== */
    if (pos_x > max_x) {
        pos_x = max_x;
        vel_x *= -0.4f;
    }
    if (pos_x < -max_x) {
        pos_x = -max_x;
        vel_x *= -0.4f;
    }
    if (pos_y > max_y) {
        pos_y = max_y;
        vel_y *= -0.4f;
    }
    if (pos_y < -max_y) {
        pos_y = -max_y;
        vel_y *= -0.4f;
    }

    uint16_t new_cx = center_x + (int16_t)pos_x;
    uint16_t new_cy = center_y + (int16_t)pos_y;

    /* ===== 屏幕边界保护（防止圆画出界） ===== */
    if (new_cx < RADIUS) new_cx = RADIUS;
    if (new_cx > spilcddev.width - RADIUS) new_cx = spilcddev.width - RADIUS;
    if (new_cy < RADIUS) new_cy = RADIUS;
    if (new_cy > spilcddev.height - RADIUS) new_cy = spilcddev.height - RADIUS;

    /* ===== 日志输出（每秒一次） ===== */
    static uint32_t last_print = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_print > 1000) {
        ESP_LOGI(TAG,
            "p=%.1f r=%.1f vel=%.2f,%.2f pos=%d,%d",
            pitch, roll, vel_x, vel_y, new_cx, new_cy);
        last_print = now;
    }

    /* ===== 无变化则跳过绘制（需先释放锁） ===== */
    if (!first_run && new_cx == last_cx && new_cy == last_cy) {
        lcd_unlock();
        return;
    }

    /* ===== 擦除旧圆（用背景色绘制，使用无锁版本） ===== */
    if (!first_run) {
        spilcd_draw_circle_nolock(last_cx, last_cy, RADIUS, WHITE);
    }

    /* ===== 绘制新圆（用前景色绘制，使用无锁版本） ===== */
    spilcd_draw_circle_nolock(new_cx, new_cy, RADIUS, BLACK);

    last_cx = new_cx;
    last_cy = new_cy;
    first_run = false;

    lcd_unlock();
}

void circle_show(void)
{
    spilcd_clear(WHITE);

    center_x = spilcddev.width / 2;
    center_y = spilcddev.height / 2;

    last_cx = center_x;
    last_cy = center_y;
    first_run = true;

    /* ===== 重置状态 ===== */
    pos_x = 0;
    pos_y = 0;
    vel_x = 0;
    vel_y = 0;

    circle_update();
}

