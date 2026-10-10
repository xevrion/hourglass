#pragma once

#include "qmi8658.h"

class Imu {
public:
    bool begin();
    bool read(float& ax, float& ay, float& az);

private:
    qmi8658_dev_t dev_{};
};
