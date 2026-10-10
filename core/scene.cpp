#include "scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "font.hpp"

namespace hg {

namespace {

constexpr int kTicks = 120;
constexpr float kRimInner = 214.0f;
constexpr float kRimOuter = 226.0f;
constexpr float kCentre = kScreen / 2.0f - 0.5f;

constexpr uint16_t kBlack = 0;
constexpr uint16_t kGlass = rgb565(90, 90, 100);
constexpr uint16_t kTrack = rgb565(38, 38, 44);
constexpr uint16_t kDim = rgb565(150, 150, 160);

struct Palette {
    uint8_t lit[3];
    uint8_t sand[4][3];
};

const Palette kFocus = {{255, 176, 46}, {{255, 176, 46}, {244, 160, 38}, {255, 192, 72}, {232, 146, 30}}};
const Palette kRest = {{64, 224, 200}, {{64, 224, 200}, {48, 206, 186}, {96, 236, 214}, {36, 190, 170}}};

const Palette& palette_for(Phase p) { return p == Phase::Focus ? kFocus : kRest; }

uint16_t scaled(const uint8_t c[3], float k) {
    return rgb565(uint8_t(c[0] * k), uint8_t(c[1] * k), uint8_t(c[2] * k));
}

bool is_edge(const Sand& s, int x, int y) {
    return !s.inside(x - 1, y) || !s.inside(x + 1, y) || !s.inside(x, y - 1) || !s.inside(x, y + 1);
}

// A box laid out for an upright board, turned half a revolution for a flipped one.
Rect place(int x, int y, int w, int h, bool flipped) {
    if (flipped) return {kScreen - x - w, kScreen - y - h, kScreen - x - 1, kScreen - y - 1};
    return {x, y, x + w - 1, y + h - 1};
}

}  // namespace

Scene::Scene() : shown_(kGrid * kGrid, 0), ticks_(kTicks), tick_colour_(kTicks, -1) {
    for (int t = 0; t < kTicks; ++t) {
        const float a = (-90.0f + float(t) * 360.0f / kTicks) * 3.14159265f / 180.0f;
        const float ca = std::cos(a), sa = std::sin(a);
        Pixels& px = ticks_[t];
        for (float r = kRimInner; r <= kRimOuter; r += 0.5f) {
            for (float w = -1.5f; w <= 1.5f; w += 0.5f) {
                px.emplace_back(int16_t(std::lround(kCentre + r * ca - w * sa)), int16_t(std::lround(kCentre + r * sa + w * ca)));
            }
        }
        std::sort(px.begin(), px.end());
        px.erase(std::unique(px.begin(), px.end()), px.end());
    }
}

void Scene::paint_cell(Surface& sf, const Sand& s, int x, int y, uint8_t shade) {
    uint16_t c = kBlack;
    if (shade) c = scaled(palette_for(phase_).sand[shade - 1], 1.0f);
    else if (is_edge(s, x, y) && s.inside(x, y)) c = kGlass;
    sf.fill_rect(kOrigin + x * kCellPx, kOrigin + y * kCellPx, kCellPx, kCellPx, c);
}

void Scene::repaint_all(Surface& sf, const View& v) {
    sf.fill_rect(0, 0, kScreen, kScreen, kBlack);
    const Sand& s = *v.sand;
    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            const uint8_t shade = s.shade(x, y);
            shown_[y * kGrid + x] = shade;
            if (shade || (s.inside(x, y) && is_edge(s, x, y))) paint_cell(sf, s, x, y, shade);
        }
    }
    std::fill(tick_colour_.begin(), tick_colour_.end(), -1);
    text_sig_.clear();
    text_rects_.clear();
}

void Scene::update_sand(Surface& sf, const View& v) {
    const Sand& s = *v.sand;
    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            const uint8_t now = s.shade(x, y);
            uint8_t& was = shown_[y * kGrid + x];
            if (now == was) continue;
            was = now;
            paint_cell(sf, s, x, y, now);
        }
    }
}

void Scene::update_ticks(Surface& sf, const View& v) {
    const bool flipped = v.gravity == Gravity::Up;
    const int offset = flipped ? kTicks / 2 : 0;
    const int lit_count = int(v.progress * kTicks + 0.5f);
    const Palette& pal = palette_for(v.phase);

    float pulse = 1.0f;
    if (v.state == State::Done) pulse = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(v.done_t * 5.0f));
    else if (v.state == State::Paused) pulse = 0.4f;

    for (int t = 0; t < kTicks; ++t) {
        const bool lit = v.state == State::Done || ((t + offset) % kTicks) < lit_count;
        const uint16_t c = lit ? scaled(pal.lit, pulse) : kTrack;
        if (tick_colour_[t] == c) continue;
        tick_colour_[t] = c;
        for (const auto& p : ticks_[t]) sf.set(p.first, p.second, c);
    }
}

void Scene::update_text(Surface& sf, const View& v) {
    const bool flipped = v.gravity == Gravity::Up;
    const int secs = int(std::ceil(v.remaining));
    char clock[16];
    std::snprintf(clock, sizeof clock, "%02d:%02d", std::min(secs / 60, 99), secs % 60);
    const char* label = v.phase == Phase::Focus ? "FOCUS" : "BREAK";
    const char* status = v.state == State::Paused ? "PAUSED" : v.state == State::Done ? "DONE" : v.state == State::Ready ? "FLIP" : "";
    const int dots = v.completed == 0 ? 0 : (v.completed - 1) % 4 + 1;

    char power[8] = "";
    if (v.battery >= 0) std::snprintf(power, sizeof power, "%d%%%s", std::min(v.battery, 100), v.charging ? "+" : "");

    char sig[96];
    std::snprintf(sig, sizeof sig, "%s|%s|%s|%d|%d|%d|%s", clock, label, status, dots, flipped, int(v.phase), power);
    if (text_sig_ == sig) return;
    text_sig_ = sig;

    for (const Rect& r : text_rects_) sf.fill_rect(r.x0, r.y0, r.x1 - r.x0 + 1, r.y1 - r.y0 + 1, kBlack);
    text_rects_.clear();

    const Palette& pal = palette_for(v.phase);
    const uint16_t accent = scaled(pal.lit, 1.0f);
    const int cy = kScreen / 2;

    auto put = [&](int x, int y, const char* s, int scale, uint16_t colour) {
        const int w = text_width(s, scale), h = text_height(scale);
        if (!w) return;
        const Rect r = place(x, y, w, h, flipped);
        draw_text(sf, r.x0, r.y0, s, scale, colour, flipped);
        text_rects_.push_back(r);
    };

    put(288, cy - 17, clock, 5, accent);
    put(288, cy + 30, status, 3, kDim);
    const int lw = text_width(label, 3);
    put(180 - lw, cy - 10, label, 3, kDim);

    const uint16_t power_colour = v.charging ? rgb565(90, 220, 120) : v.battery <= 15 ? rgb565(255, 90, 90) : kDim;
    put(180 - text_width(power, 3), cy + 46, power, 3, power_colour);

    for (int i = 0; i < 4; ++i) {
        const Rect r = place(180 - (4 - i) * 15 + 6, cy + 24, 9, 9, flipped);
        sf.fill_rect(r.x0, r.y0, 9, 9, i < dots ? accent : kTrack);
        text_rects_.push_back(r);
    }
}

void Scene::draw(Surface& sf, const View& v) {
    if (first_ || v.phase != phase_) {
        phase_ = v.phase;
        first_ = false;
        repaint_all(sf, v);
    }
    update_sand(sf, v);
    update_ticks(sf, v);
    update_text(sf, v);
}

}  // namespace hg
