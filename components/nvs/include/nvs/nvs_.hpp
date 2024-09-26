#pragma once

#include "nvs_enums.hpp"
#include "sdkconfig.h" // Configuration variables
#include "system_.hpp"

#include <stddef.h> // Standard libraries
#include <stdbool.h>
#include <sstream>

#include <esp_log.h>
#include <esp_err.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_check.h"

#include "logging/logging_.hpp"
#include "diagnostics/diagnostics_.hpp"

/* Forward Declarations */
class System;
class Logging;

extern "C"
{
    class NVS : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        static NVS *getInstance() // Enforce use of System as a singleton object
        {
            static NVS nvsInstance;
            return &nvsInstance;
        }

        /* NVS */
        void eraseNVSPartition(const char str[] = NVS_DEFAULT_PART_NAME);
        void eraseNVSNamespace(char str[]);

        esp_err_t openNVSStorage(const char *);
        esp_err_t closeNVStorage(void);

        esp_err_t readBooleanFromNVS(const char *, bool *);
        esp_err_t writeBooleanToNVS(const char *, bool);

        esp_err_t readStringFromNVS(const char *, std::string *);
        esp_err_t writeStringToNVS(const char *, std::string *);

        esp_err_t readU8IntegerFromNVS(const char *, uint8_t *);
        esp_err_t writeU8IntegerToNVS(const char *, uint8_t);

        esp_err_t readI32IntegerFromNVS(const char *, int32_t *);
        esp_err_t writeI32IntegerToNVS(const char *, int32_t);

        esp_err_t readU32IntegerFromNVS(const char *, uint32_t *);
        esp_err_t writeU32IntegerToNVS(const char *, uint32_t);

        /* Error storage and retreival routines (currently not used inside this project) */
        // uint8_t getErrorCount(void);
        // esp_err_t readErrorStringFromNVS(std::string *strValue);
        // esp_err_t writeErrorStringToNVS(std::string *strValue);
        // void clearErrorBuffer(void);

        /* NVS_Diagnostics */
        void printNVS(void);

    private:
        NVS(void);
        NVS(const NVS &) = delete;            // Disable copy constructor
        void operator=(NVS const &) = delete; // Disable assignment operator

        char TAG[6] = "_nvs ";

        /* Object References */
        System *sys = nullptr;

        uint8_t show = 0;
        void setFlags(void);
        void setLogLevels(void);
        void createSemaphores(void);
        void restoreVariablesFromNVS(void);
        void initializeNVS(void);

        nvs_handle_t nvsHandle = 0;

        // uint8_t firstIndex = 0; // Error indexes (used for saving Error) but not in use inside this project.
        // uint8_t lastIndex = 0;  //
    };
}