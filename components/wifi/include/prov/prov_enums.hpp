#pragma once

#include <stdint.h> // Standard libraries

#include "esp_wifi_types.h"

/* Run Notifications and Commands */
// Task Notifications should be used for notifications or commands which need no input and return no data.
enum class PROV_NOTIFY : uint8_t
{
    CMD_PRINT_TASK_INFO = 1,
    CMD_LOG_TASK_INFO,
};

/* Class Operations */
enum class PROV_OP : uint8_t
{
    Run,
    Wait,
    Error,
    Idle,
};

enum class PROV_RUN : uint8_t
{
    Start,
    Register_Event_Handlers,
    Create_Netif_Objects,
    Wifi_Init,
    Set_Mode,
    Init_Provisioning_Manager,
    Start_Provisioning,
    Process_Wait,
    Unregister_Endpoints,
    Stop,
    Stop_Wait,
    Deinitialize,
    Deinitalization_Wait,
    Stop_Destroy_Unregister,
    Finished,
    Error,
    Idle,
};

struct PROV_Event
{
    const char *event_base;
    int32_t event_id;
    int32_t data[sizeof(wifi_sta_config_t)]; // Currently, the largest data received is that of a wifi_sta_config_t
};