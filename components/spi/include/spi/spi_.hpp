#pragma once

#include "spi_defs.hpp"

#include "esp_log.h" // ESP Libraries

#include "freertos/semphr.h" // RTOS Libraries
#include "freertos/task.h"

#include "driver/spi_common.h"

#include "spi/spi_defs.hpp" // Local definitions, structs, and enumerations

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
    //
    // SPI Bus
    //
    // SPI2 Default Pins for IO_MUX Operation (ESP32-S2/S3)
    // GPIO11  MOSI / DI
    // GPIO13  MISO / DO
    // GPIO12  Clock
    // GPIO10  Chip Select Device 0
    //
    // SPI2 Default Pins for IO_MUX Operation (ESP32-C2/C3)
    // GPIO7   MOSI / DI
    // GPIO2   MISO / DO
    // GPIO6   Clock
    // GPIO10  Chip Select Device 0

    // No Direct Routing in IO_MUX for SPI3 in any device

    class SPI  : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        SPI(spi_host_device_t, int, int, int);
        ~SPI();

        TaskHandle_t &getRunTaskHandle(void);
        QueueHandle_t &getCmdRequestQueue(void);

    private:
        //
        // Private variables
        //
        char TAG[6] = "_spi ";

        /* Object References */
        System *sys = nullptr;
        NVS *nvs = nullptr;

        spi_host_device_t spiHost; //
        int spiMOSIPin = 0;        // Initial values
        int spiMISOPin = 0;        //
        int spiClockPin = 0;       //

        /* Taks Handles that we might need */
        TaskHandle_t taskHandleSystemRun = nullptr;

        uint8_t show = 0;
        uint8_t showSPI = 0;

        void setFlags(void); // Standard Pre-Task Functions
        void setLogLevels(void);
        void createSemaphores(void);
        void destroySemaphores(void);
        void createQueues(void);
        void destroyQueues(void);

        //
        // RTOS Related variables/functions
        //
        uint8_t runStackSizeK = 8;           // Default/Minimum stacksize
        TaskHandle_t taskHandleRun = nullptr;
        QueueHandle_t xQueueSPICmdRequests;
        SPI_CmdRequest *ptrSPICmdReq;
        SPI_RESPONSE *ptrSPICmdResp;

        // spi_cmd_handle_t spi_cmd_handle = nullptr;

        uint32_t ticksToWait; // Timeout in ticks for read and write

        SPI_OP spiOP = SPI_OP::Run;
        SPI_INIT initSPIStep = SPI_INIT::Finished;

        //
        // Direct I2C functions
        //
        void readBytesRegAddr(uint8_t, uint8_t, size_t, uint8_t *, int32_t);
        void writeBytesRegAddr(uint8_t, uint8_t, size_t, uint8_t *, int32_t);

        void readBytesImmediate(uint8_t, size_t, uint8_t *, int32_t);
        void writeBytesImmediate(uint8_t, size_t, uint8_t *, int32_t);

        QueueHandle_t queueCmdRequests = nullptr;   // SPI <-- (Incomming commands arrive here)
        SPI_CmdRequest *ptrSPICmdRequest = nullptr; //

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
}