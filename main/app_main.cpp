#include <cmath>

#include "bsp/esp32_s3_touch_amoled_1_75c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gesture.hpp"
#include "imu.hpp"
#include "panel.hpp"
#include "scene.hpp"
#include "session.hpp"

static const char* TAG = "hourglass";

constexpr int kFrameMs = 33;
constexpr float kTilt = 2.0f;      // m/s^2 along the sensor's x axis before gravity switches
constexpr float kSideTilt = 3.0f;  // m/s^2 along y before the stream leans
constexpr float kShake = 8.0f;     // m/s^2 away from 1 g that counts as a shake

// Flip these if the sand falls the wrong way when the board is held upright or tilted.
constexpr bool kInvertGravity = false;
constexpr bool kInvertLateral = false;

// The adapter treats a timeout of 0 as "do not wait", so only -1 reliably takes the lock.
struct DisplayLock {
    DisplayLock() { bsp_display_lock(-1); }
    ~DisplayLock() { bsp_display_unlock(); }
};

// The sensor's +x points away from the USB-C port, which is the top of the screen,
// so it reads positive when the board is held upright and the sand should rest low.
static hg::Gravity pick_gravity(float ax, hg::Gravity current) {
    if (kInvertGravity) ax = -ax;
    if (ax > kTilt) return hg::Gravity::Down;
    if (ax < -kTilt) return hg::Gravity::Up;
    return current;
}

static int pick_lateral(float ay) {
    if (kInvertLateral) ay = -ay;
    if (ay > kSideTilt) return 1;
    if (ay < -kSideTilt) return -1;
    return 0;
}

extern "C" void app_main() {
    if (!bsp_display_start()) {
        ESP_LOGE(TAG, "display failed to start");
        return;
    }

    Panel panel;
    {
        DisplayLock lock;
        lv_obj_t* screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        if (!panel.begin(screen)) {
            ESP_LOGE(TAG, "out of memory for the canvas");
            return;
        }
    }
    bsp_display_brightness_set(80);

    hg::Sand sand;
    hg::Session session(sand);
    hg::Scene scene;
    hg::Gesture gesture;
    hg::Gravity gravity = hg::Gravity::Down;
    session.restart(gravity);

    Imu imu;
    const bool have_imu = imu.begin();
    lv_indev_t* touch = bsp_display_get_input_dev();

    TickType_t wake = xTaskGetTickCount();
    int frame = 0;
    float ax = 0, ay = 0, az = 0;
    for (;;) {
        if (have_imu) imu.read(ax, ay, az);
        gravity = pick_gravity(ax, gravity);
        const int lateral = pick_lateral(ay);
        if (std::fabs(std::sqrt(ax * ax + ay * ay + az * az) - 9.8f) > kShake) sand.stir(35);

        bool pressed = false;
        lv_point_t point = {0, 0};
        if (touch) {
            DisplayLock lock;
            pressed = lv_indev_get_state(touch) == LV_INDEV_STATE_PRESSED;
            lv_indev_get_point(touch, &point);
        }
        const hg::Tap tap = gesture.update(pressed, point.x, point.y, kFrameMs / 1000.0f);
        if (tap == hg::Tap::Short) session.toggle_pause();
        if (tap == hg::Tap::Long) {
            if (session.state() == hg::State::Ready) session.cycle_preset(gravity);
            else session.restart(gravity);
        }

        // Two steps per frame, so a grain falls about sixty cells a second.
        session.update(1.0f / 60.0f, gravity, lateral);
        session.update(1.0f / 60.0f, gravity, lateral);

        const hg::View view{&sand, session.state(), session.phase(), gravity, session.remaining_seconds(gravity),
                            session.progress(gravity), session.completed(), session.since_done()};
        {
            DisplayLock lock;
            scene.draw(panel.surface(), view);
            panel.present();
        }

        if (++frame % 30 == 0) {
            ESP_LOGI(TAG, "accel %.1f %.1f %.1f  %s  state %d  left %.0fs", ax, ay, az,
                     gravity == hg::Gravity::Down ? "down" : "up", int(session.state()), view.remaining);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(kFrameMs));
    }
}
