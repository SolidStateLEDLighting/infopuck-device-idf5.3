#pragma once

#include "i2c_defs.hpp"

#include "esp_log.h" // ESP Libraries
#include "driver/i2c.h"

#include "freertos/semphr.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOSConfig.h"


#ifdef __cplusplus
extern "C"
{
#endif
    class System;

    extern SemaphoreHandle_t semI2CEntry;

    //
    // I2C 0
    //

    class I2C
    {
    public:
        I2C(i2c_port_t, gpio_num_t, gpio_num_t, uint32_t, uint32_t);
        ~I2C();

        QueueHandle_t &getCmdRequestQueue(void);
        void muxScan();
        void busScan();
        bool slavePresent(uint8_t, int32_t);

    private:
        //
        // Private variables
        //
        char TAG[5] = "I2C ";
        bool HasMux = false;
        //
        // RTOS Related variables/functions
        //
        TaskHandle_t taskHandleI2C = nullptr;
        QueueHandle_t xQueueI2CCmdRequests;
        I2C_CmdRequest *ptrI2CCmdReq;
        I2C_Response *ptrI2CCmdResp;

        i2c_port_t port; // Unchangable during object lifetime
        gpio_num_t i2c_sda_pin;
        gpio_num_t i2c_scl_pin;
        uint32_t clock_speed;
        uint32_t defaultTimeout;

        i2c_cmd_handle_t i2c_cmd_handle = nullptr;
        uint8_t i2c_slave_address; // Changeable during object lifetime

        bool i2c_AddressSent; // State varible to help with write protocol
        uint32_t ticksToWait; // Timeout in ticks for read and write

        I2C_OP i2cOP = I2C_OP::Run;
        I2C_INIT initI2CStep = I2C_INIT::Finished;

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
        bool showInitSteps = false;
    };

#ifdef __cplusplus
}
#endif