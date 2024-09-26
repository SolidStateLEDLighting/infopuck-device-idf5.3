#pragma once

#include "display/display_defs.hpp" // Local definitions, structs, and enumerations

#include <cstring> // Standard libraries

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"

#include "esp_log.h" // ESP libraries
#include "esp_check.h"

#include "system_.hpp"
#include "nvs/nvs_.hpp"

#include "logging/logging_.hpp"
#include "diagnostics/diagnostics_.hpp"

/* Forward Declarations */
class System;
class NVS;

extern "C"
{
    class Display : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        Display();
        ~Display();

        TaskHandle_t &getRunTaskHandle(void);    // Typically, these unsafe functions are
        QueueHandle_t &getCmdRequestQueue(void); // called at object creation and only once.

    private:
        char TAG[6] = "_disp";

        /* Object References */
        System *sys = nullptr;
        NVS *nvs = nullptr;

        /* Taks Handles that we might need */
        TaskHandle_t taskHandleSystemRun = nullptr;

        uint8_t runStackSizeK = 10; // Default/Minimum stacksize

        uint8_t show = 0;  // show Flags
        uint8_t showDisplay = 0;

        void setFlags(void); // Standard Pre-Task Functions
        void setLogLevels(void);
        void createSemaphores(void);
        void destroySemaphores(void);
        void createQueues(void);
        void destroyQueues(void);

        /* Display NVS */
        void restoreVariablesFromNVS(void);
        void saveVariablesToNVS(void);
        
        TaskHandle_t taskHandleRun = nullptr;

        QueueHandle_t queueCmdRequests = nullptr;           // DISPLAY <-- (Incomming commands arrive here)
        DISPLAY_CmdRequest *ptrDisplayCmdRequest = nullptr; //

        /* Display RUN */
        static void runMarshaller(void *);
        void run(void);

        DISPLAY_OP dispOP = DISPLAY_OP::Idle;                       // Object States
        DISPLAY_SHUTDOWN dispShdnStep = DISPLAY_SHUTDOWN::Finished; //
        DISPLAY_INIT dispInitStep = DISPLAY_INIT::Finished;         //
    };
}