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

// #include "display/display_.hpp"

#include "logging/logging_.hpp"

// class Logging;
// class Display;

extern "C"
{
    class Diagnostics
    {
    public:
        Diagnostics(){};
        virtual ~Diagnostics() = default; // Marks this class as abstract

    protected:
        void printTaskInfoByColumns(TaskHandle_t);
    };
}