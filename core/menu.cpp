#include "menu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "font.hpp"
#include "sand.hpp"

namespace hg {

namespace {

const char* const kLabels[Menu::kItems] = {"RESUME", "RESTART", "RESET ALL", "25 / 5", "50 / 10", "15 / 3", "DEMO 45S", "BRIGHTNESS"};

constexpr int kMid = kScreen / 2;
constexpr uint16_t kDimText = rgb565(120, 120, 130);
constexpr uint16_t kFaintText = rgb565(60, 60, 68);

}  // namespace

const char* Menu::label(int item) { return kLabels[std::clamp(item, 0, kItems - 1)]; }

int Menu::selected() const { return std::clamp(int(std::lround(pos_)), 0, kItems - 1); }

void Menu::open(bool flipped) {
    open_ = true;
    flipped_ = flipped;
    // The press that opened the menu is still down; ignore it until it lifts.
    wait_release_ = true;
    down_prev_ = false;
    moved_ = false;
    dirty_ = repaint_ = true;
    pos_ = target_ = 0.0f;
    idle_ = 0.0f;
}

void Menu::set_pos(float p) {
    p = std::clamp(p, 0.0f, float(kItems - 1));
    if (p != pos_) dirty_ = true;
    pos_ = p;
}

MenuAction Menu::activate() {
    switch (selected()) {
    case 0: open_ = false; return MenuAction::Resume;
    case 1: open_ = false; return MenuAction::Restart;
    case 2: open_ = false; return MenuAction::ResetAll;
    case 3: open_ = false; return MenuAction::Preset0;
    case 4: open_ = false; return MenuAction::Preset1;
    case 5: open_ = false; return MenuAction::Preset2;
    case 6: open_ = false; return MenuAction::Preset3;
    default: return MenuAction::Brightness;
    }
}

MenuAction Menu::update(bool down, int x, int y, float dt) {
    (void)x;
    if (!open_) return MenuAction::None;

    idle_ += dt;
    if (idle_ > kIdleClose) {
        open_ = false;
        return MenuAction::Resume;
    }
    if (wait_release_) {
        if (!down) wait_release_ = false;
        return MenuAction::None;
    }

    const float sign = flipped_ ? 1.0f : -1.0f;
    MenuAction out = MenuAction::None;

    if (down) {
        idle_ = 0.0f;
        if (!down_prev_) {
            moved_ = false;
            y_start_ = y_last_ = y;
        }
        if (std::abs(y - y_start_) > 12) moved_ = true;
        if (moved_) {
            set_pos(pos_ + sign * float(y - y_last_) / float(kItemH));
            target_ = pos_;
        }
        y_last_ = y;
    } else if (down_prev_) {
        if (moved_) {
            // No fling: the wheel only snaps, so a swipe of N items lands on N.
            target_ = std::clamp(std::round(pos_), 0.0f, float(kItems - 1));
        } else {
            const float row = std::round((flipped_ ? -1.0f : 1.0f) * float(y_start_ - kMid) / float(kItemH));
            if (row == 0.0f) out = activate();
            else target_ = std::clamp(float(selected()) + row, 0.0f, float(kItems - 1));
        }
    } else if (pos_ != target_) {
        const float diff = target_ - pos_;
        set_pos(std::fabs(diff) < 0.01f ? target_ : pos_ + diff * std::min(1.0f, dt * 12.0f));
    }

    down_prev_ = down;
    if (!open_ && out == MenuAction::None) out = MenuAction::Resume;
    return out;
}

void Menu::draw(Surface& sf, uint16_t accent) {
    if (!open_ || !dirty_) return;
    if (repaint_) {
        sf.fill_rect(0, 0, kScreen, kScreen, 0);
        repaint_ = false;
    } else {
        sf.fill_rect(30, 60, kScreen - 60, kScreen - 120, 0);
    }
    dirty_ = false;

    const int bar_w = 220;
    for (int sgn : {-1, 1}) {
        const int y = kMid + sgn * (kItemH / 2 - 2);
        sf.fill_rect(kMid - bar_w / 2, y, bar_w, 2, accent);
    }

    for (int i = 0; i < kItems; ++i) {
        const float d = float(i) - pos_;
        const float ad = std::fabs(d);
        if (ad > 2.3f) continue;
        const int scale = ad < 0.5f ? 4 : ad < 1.5f ? 3 : 2;
        const uint16_t colour = ad < 0.5f ? accent : ad < 1.5f ? kDimText : kFaintText;
        const int cy = kMid + int(std::lround((flipped_ ? -d : d) * float(kItemH)));
        const int w = text_width(kLabels[i], scale), h = text_height(scale);
        draw_text(sf, (kScreen - w) / 2, cy - h / 2, kLabels[i], scale, colour, flipped_);
    }
}

}  // namespace hg
