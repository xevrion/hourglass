#include "sand.hpp"

#include <algorithm>
#include <cmath>

namespace hg {

namespace {

int half_width(int d) {
    const float cone = 1.0f + 0.55f * float(d);
    const float cap = std::sqrt(float(kRadius * kRadius - d * d)) - 1.0f;
    return int(std::min(cone, cap));
}

}  // namespace

Sand::Sand(uint32_t seed)
    : inside_(kGrid * kGrid, 0), cells_(kGrid * kGrid, 0), rng_(seed ? seed : 1) {
    for (int y = 0; y < kGrid; ++y) {
        const int d = std::abs(y - kCenter);
        if (d >= kRadius) continue;
        const int w = half_width(d);
        for (int x = kCenter - w; x <= kCenter + w; ++x) inside_[index(x, y)] = 1;
    }
}

uint32_t Sand::next() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

bool Sand::inside(int x, int y) const {
    if (x < 0 || x >= kGrid || y < 0 || y >= kGrid) return false;
    return inside_[index(x, y)] != 0;
}

void Sand::set_interval(float seconds) { interval_ = std::max(seconds, 0.001f); }

void Sand::fill(Gravity g) {
    std::fill(cells_.begin(), cells_.end(), 0);
    tokens_ = 0.0f;
    const int dir = int(g);
    int placed = 0;
    for (int d = kRadius - 1; d >= 1 && placed < kGrains; --d) {
        const int y = kCenter + dir * d;
        const int w = half_width(d);
        // Centre-out so a partial row stays symmetric.
        for (int k = 0; k <= 2 * w && placed < kGrains; ++k) {
            const int x = kCenter + ((k & 1) ? (k + 1) / 2 : -(k / 2));
            cells_[index(x, y)] = uint8_t(1 + next() % 4);
            ++placed;
        }
    }
}

int Sand::total() const {
    int n = 0;
    for (uint8_t c : cells_) n += c != 0;
    return n;
}

int Sand::upstream(Gravity g) const {
    const int dir = int(g);
    int n = 0;
    for (int y = 0; y < kGrid; ++y) {
        if ((y - kCenter) * dir >= 0) continue;
        for (int x = 0; x < kGrid; ++x) n += cells_[index(x, y)] != 0;
    }
    return n;
}

bool Sand::move(int x, int y, int nx, int ny) {
    if (!inside(nx, ny) || cells_[index(nx, ny)]) return false;
    // One grain per token may enter the neck row, which is what sets the pace.
    if (ny == kCenter) {
        if (tokens_ < 1.0f) return false;
        tokens_ -= 1.0f;
    }
    cells_[index(nx, ny)] = cells_[index(x, y)];
    cells_[index(x, y)] = 0;
    return true;
}

void Sand::settle(int x, int y, int dir) {
    const int ny = y + dir;
    if (move(x, y, x, ny)) return;
    const int side = (next() & 1) ? 1 : -1;
    if (move(x, y, x + side, ny)) return;
    move(x, y, x - side, ny);
}

void Sand::step(float dt, Gravity g) {
    // Capped at two, not one: with a cap of one the fractional remainder is thrown
    // away every step and a 60 Hz loop passes grains at about 60% of the set rate.
    tokens_ = std::min(2.0f, tokens_ + dt / interval_);
    const int dir = int(g);
    flip_ = !flip_;
    // Scan from the downstream side so a grain moves at most one cell per step.
    const int start = dir > 0 ? kGrid - 2 : 1;
    const int stop = dir > 0 ? -1 : kGrid;
    for (int y = start; y != stop; y -= dir) {
        for (int i = 0; i < kGrid; ++i) {
            const int x = flip_ ? kGrid - 1 - i : i;
            if (cells_[index(x, y)]) settle(x, y, dir);
        }
    }
}

}  // namespace hg
