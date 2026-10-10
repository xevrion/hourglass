#pragma once

#include "surface.hpp"

namespace hg {

enum class MenuAction { None, Resume, Restart, ResetAll, Preset0, Preset1, Preset2, Preset3, Brightness };

// A vertical wheel of items. Swipe to scroll, tap the centred row to choose, tap
// a neighbouring row to step to it. It paints over the whole surface while open.
class Menu {
public:
    static constexpr int kItems = 8;
    static constexpr int kItemH = 64;
    static constexpr float kIdleClose = 8.0f;

    void open(bool flipped);
    void close() { open_ = false; }
    bool is_open() const { return open_; }
    int selected() const;
    static const char* label(int item);

    // Raw touch in panel coordinates. Returns an action when something was chosen.
    MenuAction update(bool down, int x, int y, float dt);
    void draw(Surface& sf, uint16_t accent);

private:
    MenuAction activate();
    void set_pos(float p);

    bool open_ = false;
    bool flipped_ = false;
    bool wait_release_ = false;
    bool down_prev_ = false;
    bool moved_ = false;
    bool dirty_ = true;
    bool repaint_ = true;
    float pos_ = 0.0f;
    float target_ = 0.0f;
    float idle_ = 0.0f;
    int y_start_ = 0;
    int y_last_ = 0;
};

}  // namespace hg
