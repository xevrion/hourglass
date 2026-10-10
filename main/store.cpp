#include "store.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "store";

static bool open_nvs(nvs_open_mode_t mode, nvs_handle_t& handle) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs unavailable: %s", esp_err_to_name(err));
        return false;
    }
    return nvs_open("hourglass", mode, &handle) == ESP_OK;
}

Saved store_load() {
    Saved s;
    nvs_handle_t h;
    if (!open_nvs(NVS_READONLY, h)) return s;
    uint8_t v = 0;
    if (nvs_get_u8(h, "preset", &v) == ESP_OK) s.preset = v;
    if (nvs_get_u8(h, "done", &v) == ESP_OK) s.completed = v;
    if (nvs_get_u8(h, "bright", &v) == ESP_OK) s.brightness = v;
    nvs_close(h);
    return s;
}

void store_save(const Saved& s) {
    nvs_handle_t h;
    if (!open_nvs(NVS_READWRITE, h)) return;
    nvs_set_u8(h, "preset", uint8_t(s.preset));
    nvs_set_u8(h, "done", uint8_t(s.completed > 255 ? 255 : s.completed));
    nvs_set_u8(h, "bright", uint8_t(s.brightness));
    nvs_commit(h);
    nvs_close(h);
}
