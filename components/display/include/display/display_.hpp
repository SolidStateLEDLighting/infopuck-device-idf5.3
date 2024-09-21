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

/* Forward Declarations */
class System;
class NVS;

extern "C"
{
    class Display
    {
    public:
        Display();
        ~Display();

        TaskHandle_t &getRunTaskHandle(void);    // Typically, these unsafe functions are
        QueueHandle_t &getCmdRequestQueue(void); // called at object creation and only once.

        /* Display Diagnostics */
        void printTaskInfoByColumns(void);

    private:
        char TAG[6] = "_disp";

        /* Object References */
        System *sys = nullptr;
        NVS *nvs = nullptr;

        /* Taks Handles that we might need */
        TaskHandle_t taskHandleSystemRun = nullptr;

        uint8_t show = 0;
        uint8_t showDisplay = 0;

        void setFlags(void); // Standard Pre-Task Functions
        void setLogLevels(void);
        void createSemaphores(void);
        void destroySemaphores(void);
        void createQueues(void);
        void destroyQueues(void);

        /* Display Diagnostics*/
        void logTaskInfo(void);

        /* Display Logging */
        std::string errMsg = "";
        void routeLogByRef(LOG_TYPE, std::string *);
        void routeLogByValue(LOG_TYPE, std::string);

        /* Display NVS */
        void restoreVariablesFromNVS(void);
        void saveVariablesToNVS(void);

        uint8_t runStackSizeK = 10; // Default/Minimum stacksize
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