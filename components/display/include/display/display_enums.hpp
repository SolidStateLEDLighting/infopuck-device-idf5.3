#pragma once

#include <stdint.h> // Standard libraries
#include <string>

/* Run Notifications and Commands */
// Task Notifications should be used for notifications or commands which need no input and return no data.
enum class DISPLAY_NOTIFY : uint8_t
{
    NFY_EMPTY = 1, // NFY are notifications
    CMD_EMPTY,     // CMD are very simple commands without parameters
    CMD_LOG_TASK_INFO,
    CMD_SHUT_DOWN,
};

// Queue based commands should be used for commands which may provide input and perhaps return data.
enum class DISPLAY_COMMAND : uint8_t
{
    NONE = 0,
    DO_SOMETHING = 0,
};

struct DISPLAY_CmdRequest // The expected format of any incoming command
{
    DISPLAY_COMMAND requestedCmd; //
    uint8_t data[32];             //
    uint8_t dataLength;           //
};

/* Class Operations */
enum class DISPLAY_OP : uint8_t
{
    Run,
    Shutdown,
    Init,
    Error,
    Idle,
};

enum class DISPLAY_SHUTDOWN : uint8_t
{
    Start,
    Finished,
    Error,
};

enum class DISPLAY_INIT : uint8_t
{
    Start,
    Finished,
    Error,
};

enum class DISPLAY_RUN : uint8_t
{
    Start,
    Finished,
    Error,
    Idle,
};

/* Display States */
enum class DISPLAY_STATE : uint8_t
{
    NONE = 0,
    ON,
    OFF,
};