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

    extern SemaphoreHandle_t semSPIEntry;

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

    class SPI
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
        char TAG[5] = "_spi";
        bool HasMux = false;

        spi_host_device_t spiHost;
        int spiMOSIPin = 12;
        int spiMISOPin = 12;
        int spiClockPin = 10;

        //
        // RTOS Related variables/functions
        //
        TaskHandle_t taskHandleRun = nullptr;
        QueueHandle_t xQueueSPICmdRequests;
        SPI_CmdRequest *ptrSPICmdReq;
        SPI_Response *ptrSPICmdResp;

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