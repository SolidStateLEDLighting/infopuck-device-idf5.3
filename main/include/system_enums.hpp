#pragma once

#include <stddef.h> // Standard libraries
#include <stdint.h>
#include <string>

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/queue.h"

//
// Run Notifications and Commands
//
// Task Notifications should be used for notifications or commands which need no input and return no data.
enum class SYS_NOTIFY : uint8_t // Task Notification definitions for the Run loop
{
    NFY_WIFI_CONNECTING = 1, // We have started the process to connect
    NFY_WIFI_CONNECTED,      // Wifi can be used
    NFY_WIFI_DISCONNECTING,  // Stop using Wifi
    NFY_WIFI_DISCONNECTED,   // Wifi is availiable to be connected again
    CMD_DESTROY_WIFI,        // Receives a request to destroy Wifi
};

// Queue based commands should be used for commands which may provide input and perhaps return data.
enum class SYS_COMMAND : uint8_t
{
    NONE = 0,
};

//
// Message Operations
//
enum class SYS_STATUS : uint8_t
{
    DATA_OK,
    DATA_EMPTY,
    ERROR,
};

struct SYS_CmdRequest
{
    QueueHandle_t queueToSendResponse; // 4 bytes.   If NULL, no response will be sent.
    SYS_COMMAND requestedCmd;
    std::string *stringData = nullptr;
    int64_t data64bit = 0;
};

struct SYS_Response
{
    SYS_STATUS responseCode;
    std::string *jsonResponse = nullptr;
};

//
// Class Operations
//
enum class SYS_OP : uint8_t
{
    Run,
    Shutdown,
    Init,
    Error,
    Idle,
};

enum class SYS_SHUTDOWN : uint8_t
{
    Start,
    Finished,
    Error,
};

enum class SYS_INIT : uint8_t
{
    Start,
    Power_Down_Unused_Resources,
    Start_Network_Interface,
    Create_Default_Event_Loop,
    Start_GPIO,

    // Create_Speaker,
    // Wait_On_Speaker,
    // Create_Mic,
    // Wait_On_Mic,

    Create_I2C,
    Wait_On_I2C,
    // Create_Touch,
    // Wait_On_Touch,
    // Create_IMU,
    // Wait_On_IMU,

    // Create_SPI,
    // Wait_On_SPI,
    // Create_Display,
    // Wait_On_Display,

    Create_Wifi,
    Wait_On_Wifi,
    Start_System_Timer,
    Finished,
    Error,
};

//
// Logging
//
enum class LOG_TYPE : uint8_t
{
    ERROR = 1,
    WARN,
    INFO,
};

//
// Testing
//
enum class SYS_TEST_TYPE : uint8_t
{
    IDLE,
    LIFE_CYCLE_CREATE,
    LIFE_CYCLE_DESTROY,
    POWER_MANAGEMENT,
    LIGHT_SLEEP,
    DEEP_SLEEP,
    NVS,
    WIFI,
};