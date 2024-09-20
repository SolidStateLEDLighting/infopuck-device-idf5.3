#pragma once

#include <stddef.h> // Standard libraries
#include <stdint.h>

#include <driver/gpio.h>
#include "driver/i2c.h"

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/queue.h"

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

enum class I2C_ACTION : uint8_t;

struct I2C_CmdRequest
{
    QueueHandle_t QueueToSendResponse; // 4 bytes.   If NULL, no response will be sent.
    uint8_t busDevAddress;
    I2C_ACTION action;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
    bool debug = false;
};

struct I2C_Response
{
    I2C_ACTION result;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
};
//
// Request/Response
//
enum class I2C_ACTION : uint8_t
{
    Read_Bytes_RegAddr,
    Write_Bytes_RegAddr,
    Read_Bytes_Immediate,
    Write_Bytes_Immediate,
    Returning_Data,
    Returning_Ack,
    Returning_Nak,
    Returning_Error
};

enum class I2C_OP : uint8_t
{
    Init,
    Run,
    ReadWriteI2CBus,
};

enum class I2C_INIT : uint8_t
{
    Start,
    Load_NVS_Settings,
    Scan,
    Finished,
};
