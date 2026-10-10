#pragma once

#include <memory>

#include "lvgl.h"
#include "surface.hpp"

// A full-screen LVGL canvas that the shared scene paints into.
class Panel {
public:
    bool begin(lv_obj_t* parent);
    hg::Surface& surface() { return *surface_; }
    // Hands the painted region to LVGL. The caller holds the display lock.
    void present();

private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* buf_ = nullptr;
    std::unique_ptr<hg::Surface> surface_;
};
