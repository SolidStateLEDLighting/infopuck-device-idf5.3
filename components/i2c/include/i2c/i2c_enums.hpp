#pragma once
#include <stddef.h> // Standard libraries
#include <stdint.h>

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/queue.h"

enum class I2C_NOTIFY : uint32_t // Task Notification definitions for the Run loop
{
    NFY_EMPTY = 1,     //
    CMD_EMPTY,         // CMDs are very simple commands without parameters
    CMD_LOG_TASK_INFO, // (Calls for an action but no data)
    CMD_SHUT_DOWN,     //
};

enum class I2C_COMMAND : uint8_t; // Foreword declarations
enum class I2C_RESPONSE : uint8_t;

struct I2C_CmdRequest
{
    QueueHandle_t QueueToSendResponse; // 4 bytes.   If NULL, no response will be sent.
    uint8_t busDevAddress;
    I2C_COMMAND command;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
    bool debug = false;
};

struct I2C_CmdResponse
{
    I2C_RESPONSE response;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
};
//
// Request/Response
//
enum class I2C_COMMAND : uint8_t
{
    Read_Bytes_RegAddr = 1,
    Write_Bytes_RegAddr,
    Read_Bytes_Immediate,
    Write_Bytes_Immediate,
};

enum class I2C_RESPONSE : uint8_t
{
    Returning_Data = 1,
    Returning_Ack,
    Returning_Nak,
    Returning_Error,
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
    Create_Master_Bus,
    Scan,
    Finished,
};
