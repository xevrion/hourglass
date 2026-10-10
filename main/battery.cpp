#include "battery.hpp"

#include "bsp/esp32_s3_touch_amoled_1_75c.h"
#include "esp_log.h"

static const char* TAG = "battery";

namespace {

constexpr uint8_t kAddress = 0x34;
constexpr uint8_t kStatus1 = 0x00;    // bit 3: a battery is present
constexpr uint8_t kStatus2 = 0x01;    // bits 6:5 == 01: charging
constexpr uint8_t kAdcEnable = 0x30;  // bit 0: measure the battery voltage
constexpr uint8_t kPercent = 0xA4;

}  // namespace

bool Battery::begin() {
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kAddress,
        .scl_speed_hz = 400000,
    };
    const esp_err_t err = i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, &dev_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no power chip: %s", esp_err_to_name(err));
        dev_ = nullptr;
        return false;
    }
    return true;
}

bool Battery::reg_read(uint8_t reg, uint8_t& value) {
    return dev_ && i2c_master_transmit_receive(dev_, &reg, 1, &value, 1, 100) == ESP_OK;
}

bool Battery::reg_write(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return dev_ && i2c_master_transmit(dev_, buf, 2, 100) == ESP_OK;
}

bool Battery::read(int& percent, bool& charging) {
    uint8_t adc = 0, status1 = 0, status2 = 0, soc = 0;
    if (!adc_on_) {
        if (!reg_read(kAdcEnable, adc)) return false;
        if (!(adc & 0x01) && !reg_write(kAdcEnable, adc | 0x01)) return false;
        adc_on_ = true;
    }
    if (!reg_read(kStatus1, status1) || !reg_read(kStatus2, status2)) return false;
    charging = ((status2 >> 5) & 0x03) == 0x01;
    if (!(status1 & (1 << 3))) return false;
    if (!reg_read(kPercent, soc)) return false;
    percent = soc > 100 ? 100 : soc;
    return true;
}
