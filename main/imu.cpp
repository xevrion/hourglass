#include "imu.hpp"

#include "bsp/esp32_s3_touch_amoled_1_75c.h"
#include "esp_log.h"

static const char* TAG = "imu";

bool Imu::begin() {
    esp_err_t err = qmi8658_init(&dev_, bsp_i2c_get_handle(), QMI8658_ADDRESS_HIGH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
        return false;
    }
    qmi8658_set_accel_range(&dev_, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&dev_, QMI8658_ACCEL_ODR_125HZ);
    qmi8658_set_accel_unit_mps2(&dev_, true);
    qmi8658_enable_accel(&dev_, true);
    return true;
}

bool Imu::read(float& ax, float& ay, float& az) {
    return qmi8658_read_accel(&dev_, &ax, &ay, &az) == ESP_OK;
}
