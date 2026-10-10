#pragma once

#include <cstdlib>

namespace hg {

enum class Tap { None, Short, Long };

// Turns raw press state into taps. A short tap fires on release; a long press
// fires once, while still held, so the user sees it happen.
class Gesture {
public:
    static constexpr float kTapSeconds = 0.35f;
    static constexpr float kLongSeconds = 0.8f;
    static constexpr int kSlopPx = 24;

    Tap update(bool down, int x, int y, float dt) {
        Tap out = Tap::None;
        if (down) {
            if (!held_) {
                held_ = true, fired_ = false, held_for_ = 0.0f, x0_ = x, y0_ = y;
            }
            held_for_ += dt;
            if (std::abs(x - x0_) > kSlopPx || std::abs(y - y0_) > kSlopPx) fired_ = true;
            if (!fired_ && held_for_ >= kLongSeconds) {
                fired_ = true;
                out = Tap::Long;
            }
        } else if (held_) {
            if (!fired_ && held_for_ <= kTapSeconds) out = Tap::Short;
            held_ = false;
        }
        return out;
    }

private:
    bool held_ = false;
    bool fired_ = false;
    float held_for_ = 0.0f;
    int x0_ = 0, y0_ = 0;
};

}  // namespace hg
