#pragma once

#include <cstdint>
#include <vector>

namespace hg {

constexpr int kScreen = 466;
constexpr int kCellPx = 3;
constexpr int kGrid = 137;
constexpr int kCenter = kGrid / 2;
constexpr int kRadius = 68;
constexpr int kGrains = 1800;
constexpr int kOrigin = (kScreen - kGrid * kCellPx) / 2;

enum class Gravity : int { Down = 1, Up = -1 };

class Sand {
public:
    explicit Sand(uint32_t seed = 1);

    void fill(Gravity g);
    void set_interval(float seconds);
    // lateral is -1, 0 or +1 and makes grains slide that way when blocked below.
    void step(float dt, Gravity g, int lateral = 0);
    void stir(int percent);

    bool inside(int x, int y) const;
    uint8_t shade(int x, int y) const { return cells_[index(x, y)]; }
    int upstream(Gravity g) const;
    int total() const;
    float interval() const { return interval_; }
    float remaining(Gravity g) const { return float(upstream(g)) * interval_; }

private:
    static int index(int x, int y) { return y * kGrid + x; }

    uint32_t next();
    void settle(int x, int y, int dir, int lateral);
    bool move(int x, int y, int nx, int ny);

    std::vector<uint8_t> inside_;
    std::vector<uint8_t> cells_;
    float interval_ = 1.0f;
    float tokens_ = 0.0f;
    bool flip_ = false;
    uint32_t rng_;
};

}  // namespace hg
