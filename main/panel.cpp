#include "panel.hpp"

#include <cstring>

#include "sand.hpp"

bool Panel::begin(lv_obj_t* parent) {
    buf_ = lv_draw_buf_create(hg::kScreen, hg::kScreen, LV_COLOR_FORMAT_RGB565, 0);
    if (!buf_) return false;
    std::memset(buf_->data, 0, size_t(buf_->header.stride) * hg::kScreen);

    canvas_ = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(canvas_, buf_);
    lv_obj_center(canvas_);

    surface_ = std::make_unique<hg::Surface>(static_cast<uint8_t*>(buf_->data), hg::kScreen, hg::kScreen, int(buf_->header.stride));
    return true;
}

void Panel::present() {
    hg::Rect r;
    if (!surface_->take_dirty(r)) return;
    lv_area_t area = {r.x0, r.y0, r.x1, r.y1};
    lv_obj_invalidate_area(canvas_, &area);
}
