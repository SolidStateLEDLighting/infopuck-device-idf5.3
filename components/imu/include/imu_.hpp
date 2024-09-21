#pragma once

#include "spi_defs.hpp"

#include "esp_log.h" // ESP Libraries

#include "freertos/semphr.h" // RTOS Libraries
#include "freertos/task.h"

#include "driver/spi_common.h"

#ifdef __cplusplus
extern "C"
{
#endif
    class System;

    class IMU
    {
    public:
        IMU()
        ~IMU();

        QueueHandle_t &getCmdRequestQueue(void);

    private:
        //
        // Private variables
        //
        char TAG[5] = "IMU ";

        //
        // RTOS Related variables/functions
        //
        TaskHandle_t taskHandleIMU = nullptr;
        QueueHandle_t xQueueIMUCmdRequests;

        //
        // Private Member functions
        //
        static void runMarshaller(void *);
        void run(void);
        //
        // Debug Variables
        //
        bool showRun = false;
        bool showInitSteps = true;
    };

#ifdef __cplusplus
}
#endif
