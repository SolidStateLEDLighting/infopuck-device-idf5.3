#pragma once

#include <stddef.h> // Standard libraries
#include <stdint.h>

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/queue.h"

//
// System values
//

// enum class SPI_MODE : uint8_t
// {
//     MODE_1, // CPOLARITY=0, CPHASE=0
//     MODE_2, // CPOLARITY=0, CPHASE=1
//     MODE_3, // CPOLARITY=1, CPHASE=0
//     MODE_4, // CPOLARITY=1, CPHASE=1
// };

enum class SPI_COMMAND : uint8_t;
enum class SPI_RESPONSE : uint8_t;


struct SPI_CmdRequest
{
    QueueHandle_t QueueToSendResponse; // 4 bytes.   If NULL, no response will be sent.
    uint8_t busDevAddress;
    SPI_COMMAND command;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
    bool debug = false;
};

struct SPI_CmdResponse
{
    SPI_RESPONSE response;
    uint8_t deviceRegister;
    uint8_t dataLength;
    uint8_t data[32];
};
//
// Request/Response
//
enum class SPI_COMMAND : uint8_t
{
    Trans,
};

enum class SPI_RESPONSE : uint8_t
{
    None,
};

enum class SPI_OP : uint8_t
{
    Init,
    Run,
};

enum class SPI_INIT : uint8_t
{
    Start,
    Load_NVS_Settings,
    Init_Bus,
    Finished,
};

enum class SPI_TRANS : uint8_t
{
    Command,
    Address,
    Dummy,
    Write,
    Read,
};
