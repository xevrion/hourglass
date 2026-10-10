#pragma once

#include <algorithm>
#include <cstdint>

namespace hg {

struct Rect {
    int x0, y0, x1, y1;  // inclusive
};

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// An RGB565 pixel buffer that remembers the bounding box of everything painted
// since the last take_dirty().
class Surface {
public:
    Surface(uint8_t* base, int w, int h, int stride_bytes) : base_(base), w_(w), h_(h), stride_(stride_bytes) { clear_dirty(); }

    int width() const { return w_; }
    int height() const { return h_; }

    void set(int x, int y, uint16_t c) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        row(y)[x] = c;
        grow(x, y, x, y);
    }

    uint16_t get(int x, int y) const { return row(y)[x]; }

    void fill_rect(int x, int y, int w, int h, uint16_t c) {
        const int x0 = std::max(x, 0), y0 = std::max(y, 0);
        const int x1 = std::min(x + w, w_) - 1, y1 = std::min(y + h, h_) - 1;
        if (x1 < x0 || y1 < y0) return;
        for (int yy = y0; yy <= y1; ++yy) std::fill(row(yy) + x0, row(yy) + x1 + 1, c);
        grow(x0, y0, x1, y1);
    }

    // The bounding box widened to even start and odd end, which is what the
    // panel's address window needs. Returns false when nothing changed.
    bool take_dirty(Rect& out) {
        if (dx1_ < 0) return false;
        out = {dx0_ & ~1, dy0_ & ~1, std::min(dx1_ | 1, w_ - 1), std::min(dy1_ | 1, h_ - 1)};
        clear_dirty();
        return true;
    }

private:
    uint16_t* row(int y) const { return reinterpret_cast<uint16_t*>(base_ + size_t(y) * size_t(stride_)); }
    void clear_dirty() { dx0_ = w_, dy0_ = h_, dx1_ = dy1_ = -1; }
    void grow(int x0, int y0, int x1, int y1) {
        dx0_ = std::min(dx0_, x0), dy0_ = std::min(dy0_, y0);
        dx1_ = std::max(dx1_, x1), dy1_ = std::max(dy1_, y1);
    }

    uint8_t* base_;
    int w_, h_, stride_;
    int dx0_, dy0_, dx1_, dy1_;
};

}  // namespace hg
