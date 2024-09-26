#pragma once

#include <stdint.h> // Standard libraries
#include <string>

enum class IMU_INIT : uint8_t
{
    Start,
    Load_NVS_Settings,
    Finished,
};
