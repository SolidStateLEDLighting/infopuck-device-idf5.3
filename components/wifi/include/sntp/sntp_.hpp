#pragma once
#include "sntp/sntp_defs.hpp" // Local definitions, structs, and enumerations
#include "system_.hpp"        // Class structure and variables

#include <string> // Standard libraries

#include "esp_netif_sntp.h"

#include "esp_sntp.h"
#include "esp_netif_types.h"
#include "esp_netif_sntp.h"

#include "logging/logging_.hpp"
#include "diagnostics/diagnostics_.hpp"

/* Forward Declarations */
class System;
class NVS;

extern "C"
{
    class SNTP  : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        SNTP();
        ~SNTP();

        bool timeValid = false;

        uint8_t waitSecsTimeOut = 20; // Seconds to exire
        std::string serverName = "";

        uint32_t sntpStartTicks = 0;
        uint8_t waitingOnEpochTimeSec = 0;

        /* SNTP_Run */
        SNTP_OP sntpOP = SNTP_OP::Idle;           // Object States
        SNTP_CONN connStep = SNTP_CONN::Idle;     //
        SNTP_DISC discStep = SNTP_DISC::Finished; //

        /* SNTP_Run */
        void run(void);
        void runEvents(void);

    private:
        SNTP(const SNTP &) = delete;           // Disable copy constructor
        void operator=(SNTP const &) = delete; // Disable assignment operator

        char TAG[6] = "_sntp";

        /* Object References */
        System *sys = nullptr;
        NVS *nvs = nullptr;

        uint8_t show = 0;
        uint8_t showSNTP = 0;

        void setFlags(void);
        void setLogLevels(void);
        void createSemaphores(void);
        void createQueues(void);
        void destroySemaphores(void);
        void destroyQueues(void);

        QueueHandle_t queueEvents = nullptr;

        /* Working Variables */
        std::string timeZone = "HKT-8"; // Our default time zone selection

        esp_sntp_config_t config;
        uint8_t serverIndex = 0;

        const uint8_t waitingOnEpochTimeSecMax = 7; // We time out in 7 seconds waiting for a SNTP server response

        static void eventHandlerSNTPMarshaller(struct timeval *);
        void eventHandlerSNTP(struct timeval *);

        /* SNTP_Logging */
        std::string errMsg = "";
        void routeLogByValue(LOG_TYPE, std::string);

        /* SNTP_NVS */
        void restoreVariablesFromNVS(void);
        void saveVariablesToNVS(void);
    };
}