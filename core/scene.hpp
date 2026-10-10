#pragma once

#include <string>
#include <utility>
#include <vector>

#include "sand.hpp"
#include "session.hpp"
#include "surface.hpp"

namespace hg {

// View::battery when there is no battery fitted but the board is running from the cable.
constexpr int kOnUsb = -2;

struct View {
    const Sand* sand;
    State state;
    Phase phase;
    Gravity gravity;
    float remaining;  // seconds
    float progress;   // 0..1 of the phase used up
    int completed;    // focus sessions finished
    float done_t;     // seconds since the phase ended
    int battery;      // percent, kOnUsb, or -1 to show nothing
    bool charging;
};

// Draws the glass, the sand, the rim of ticks and the clock into a Surface. Only
// what changed since the previous call is painted, so Surface's dirty box stays small.
class Scene {
public:
    Scene();
    void draw(Surface& sf, const View& v);
    // Forces a full repaint on the next draw, for when something else used the surface.
    void invalidate() { first_ = true; }

private:
    using Pixels = std::vector<std::pair<int16_t, int16_t>>;

    void repaint_all(Surface& sf, const View& v);
    void paint_cell(Surface& sf, const Sand& s, int x, int y, uint8_t shade);
    void update_sand(Surface& sf, const View& v);
    void update_ticks(Surface& sf, const View& v);
    void update_text(Surface& sf, const View& v);

    std::vector<uint8_t> shown_;
    std::vector<Pixels> ticks_;
    std::vector<int32_t> tick_colour_;
    std::string text_sig_;
    std::vector<Rect> text_rects_;
    bool first_ = true;
    Phase phase_ = Phase::Focus;
};

}  // namespace hg
