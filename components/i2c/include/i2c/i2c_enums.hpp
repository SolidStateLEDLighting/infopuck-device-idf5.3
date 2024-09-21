#pragma once

#include <stddef.h> // Standard libraries
#include <stdint.h>

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/queue.h"

enum class I2C_ACTION : uint8_t; // Foreword declaration

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
