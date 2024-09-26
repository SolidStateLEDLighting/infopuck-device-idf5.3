#pragma once
#include <stdint.h> // Standard libraries
#include <string>

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"

#include "esp_log.h" // ESP libraries
#include "esp_check.h"

extern "C"
{
    class Logging
    {
    public:
        Logging(){};
        virtual ~Logging() = default; // Marks this class as abstract

    protected:
        std::string errMsg = "";
        void logByValue(esp_log_level_t, SemaphoreHandle_t, char[6], std::string);
        void logTaskInfo(SemaphoreHandle_t, char *);
    };
}