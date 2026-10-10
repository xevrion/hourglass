#include <cmath>

#include "bsp/esp32_s3_touch_amoled_1_75c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.hpp"
#include "gesture.hpp"
#include "imu.hpp"
#include "menu.hpp"
#include "panel.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "store.hpp"

static const char* TAG = "hourglass";

constexpr int kFrameMs = 33;
constexpr float kDt = kFrameMs / 1000.0f;
constexpr float kTilt = 2.0f;      // m/s^2 along the sensor's x axis before gravity switches
constexpr float kSideTilt = 3.0f;  // m/s^2 along y before the stream leans
constexpr float kShake = 8.0f;     // m/s^2 away from 1 g that counts as a shake
constexpr float kDimAfter = 30.0f;  // seconds without touching it before the screen dims
constexpr int kDimLevel = 8;        // percent
constexpr int kBatteryEvery = 150;  // frames between battery reads

constexpr int kBrightness[] = {25, 60, 100};
constexpr int kBrightnessCount = int(sizeof(kBrightness) / sizeof(kBrightness[0]));

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

    hg::Sand sand;
    hg::Session session(sand);
    hg::Scene scene;
    hg::Gesture gesture;
    hg::Menu menu;
    hg::Gravity gravity = hg::Gravity::Down;

    Saved saved = store_load();
    saved.brightness = saved.brightness % kBrightnessCount;
    session.set_preset(saved.preset, saved.completed, gravity);
    int shown_level = -1;
    auto set_level = [&](int percent) {
        if (percent == shown_level) return;
        shown_level = percent;
        bsp_display_brightness_set(percent);
    };
    set_level(kBrightness[saved.brightness]);

    auto remember = [&]() {
        const Saved now = {session.preset(), session.completed(), saved.brightness};
        if (now.preset == saved.preset && now.completed == saved.completed) return;
        saved = now;
        store_save(saved);
    };

    Imu imu;
    const bool have_imu = imu.begin();
    Battery battery;
    const bool have_battery = battery.begin();
    lv_indev_t* touch = bsp_display_get_input_dev();

    TickType_t wake = xTaskGetTickCount();
    int frame = 0;
    float ax = 0, ay = 0, az = 0;
    int level = -1;
    bool charging = false;
    float quiet = 0.0f;
    for (;;) {
        if (have_imu) imu.read(ax, ay, az);
        const hg::Gravity before = gravity;
        gravity = pick_gravity(ax, gravity);
        const int lateral = pick_lateral(ay);
        const bool shaken = std::fabs(std::sqrt(ax * ax + ay * ay + az * az) - 9.8f) > kShake;
        if (shaken) sand.stir(35);

        bool pressed = false;
        lv_point_t point = {0, 0};
        if (touch) {
            DisplayLock lock;
            pressed = lv_indev_get_state(touch) == LV_INDEV_STATE_PRESSED;
            lv_indev_get_point(touch, &point);
        }

        quiet = (pressed || shaken || gravity != before || menu.is_open() || session.state() == hg::State::Done) ? 0.0f : quiet + kDt;
        set_level(quiet > kDimAfter ? kDimLevel : kBrightness[saved.brightness]);

        if (menu.is_open()) {
            switch (menu.update(pressed, point.x, point.y, kDt)) {
            case hg::MenuAction::Restart: session.restart(gravity); break;
            case hg::MenuAction::ResetAll: session.reset_all(gravity); break;
            case hg::MenuAction::Preset0: session.set_preset(0, 0, gravity); break;
            case hg::MenuAction::Preset1: session.set_preset(1, 0, gravity); break;
            case hg::MenuAction::Preset2: session.set_preset(2, 0, gravity); break;
            case hg::MenuAction::Preset3: session.set_preset(3, 0, gravity); break;
            case hg::MenuAction::Brightness:
                saved.brightness = (saved.brightness + 1) % kBrightnessCount;
                store_save(saved);
                set_level(kBrightness[saved.brightness]);
                break;
            default: break;
            }
            if (!menu.is_open()) scene.invalidate();
        } else {
            const hg::Tap tap = gesture.update(pressed, point.x, point.y, kDt);
            if (tap == hg::Tap::Short) session.toggle_pause();
            if (tap == hg::Tap::Long) menu.open(gravity == hg::Gravity::Up);
        }

        // Two steps per frame, so a grain falls about sixty cells a second.
        session.update(1.0f / 60.0f, gravity, lateral);
        session.update(1.0f / 60.0f, gravity, lateral);
        remember();

        if (have_battery && frame % kBatteryEvery == 0) {
            int percent = 0;
            switch (battery.read(percent, charging)) {
            case Battery::Reading::Level: level = percent; break;
            case Battery::Reading::NoBattery: level = hg::kOnUsb, charging = false; break;
            default: level = -1; break;
            }
        }

        const hg::View view{&sand, session.state(), session.phase(), gravity, session.remaining_seconds(gravity),
                            session.progress(gravity), session.completed(), session.since_done(), level, charging};
        {
            DisplayLock lock;
            if (menu.is_open()) {
                const bool focus = session.phase() == hg::Phase::Focus;
                menu.draw(panel.surface(), focus ? hg::rgb565(255, 176, 46) : hg::rgb565(64, 224, 200));
            } else {
                scene.draw(panel.surface(), view);
            }
            panel.present();
        }

        if (++frame % 300 == 0) {
            ESP_LOGI(TAG, "state %d  left %.0fs  battery %d%%%s  preset %d  done %d", int(session.state()), view.remaining, level,
                     charging ? "+" : "", session.preset(), session.completed());
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(kFrameMs));
    }
}
