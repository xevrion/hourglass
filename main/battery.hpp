#pragma once

#include "driver/i2c_master.h"

// Battery level from the board's AXP2101 power chip, read over the same I2C bus
// as the touch panel and tilt sensor.
class Battery {
public:
    enum class Reading { Level, NoBattery, Error };

    bool begin();
    Reading read(int& percent, bool& charging);

private:
    bool reg_read(uint8_t reg, uint8_t& value);
    bool reg_write(uint8_t reg, uint8_t value);

    i2c_master_dev_handle_t dev_ = nullptr;
    bool adc_on_ = false;
};
