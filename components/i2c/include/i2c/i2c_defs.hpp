#pragma once

#include "i2c/i2c_enums.hpp"

#include <driver/gpio.h>
#include "driver/i2c.h"

#define I2C_MASTER_ACK_EN true   /*!< Enable ack check for master */
#define I2C_MASTER_ACK_DIS false /*!< Disable ack check for master */

//
// Port 0 values
//
static const i2c_port_t I2C_PORT_0 = I2C_NUM_0;
static const gpio_num_t SDA_PIN_0 = GPIO_NUM_6;
static const gpio_num_t SCL_PIN_0 = GPIO_NUM_7;

//
// System values
//
static const uint32_t defaultClockSpeed = 400000; // Clock speed in Hz, default: 400KHz
static const uint32_t defaultTimeout = 500;       // Timeout in milliseconds, default: 500ms


