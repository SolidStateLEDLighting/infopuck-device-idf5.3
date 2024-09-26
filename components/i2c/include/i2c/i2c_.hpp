#pragma once
#include "i2c_defs.hpp"

#include "esp_log.h" // ESP libraries
#include "esp_check.h"

#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"

#include "freertos/semphr.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOSConfig.h"

#include "system_.hpp"
#include "logging/logging_.hpp"
#include "diagnostics/diagnostics_.hpp"

/* Forward Declarations */
class System;

extern "C"
{
    class I2C : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        I2C();
        ~I2C();

        TaskHandle_t &getRunTaskHandle(void);
        QueueHandle_t &getCmdRequestQueue(void);

        void busScan();
        bool slavePresent(uint8_t, int32_t);

    private:
        //
        // Private variables
        //
        char TAG[6] = "_i2c ";

        /* Object References */
        System *sys = nullptr;

        /* Taks Handles that we might need */
        TaskHandle_t taskHandleSystemRun = nullptr;

        uint8_t runStackSizeK = 3; // Default/Minimum stacksize

        uint8_t show = 0; // show Flags
        uint8_t showI2C = 0;

        void setFlags(void); // Standard Pre-Task Functions
        void setLogLevels(void);
        void createSemaphores(void);
        void destroySemaphores(void);
        void createQueues(void);
        void destroyQueues(void);

        //
        // RTOS Related variables/functions
        //
        TaskHandle_t taskHandleRun = nullptr;

        QueueHandle_t queueCmdRequests = nullptr;   // DISPLAY <-- (Incomming commands arrive here)
        I2C_CmdRequest *ptrI2CCmdRequest = nullptr; //
        I2C_CmdResponse *ptrI2CCmdResponse = nullptr;      //

        // QueueHandle_t xQueueI2CCmdRequests;
        // I2C_CmdRequest *ptrI2CCmdReq;
        // I2C_CmdResponse *ptrI2CCmdResp;

        i2c_port_t i2c_port = I2C_NUM_0;

        i2c_master_bus_handle_t bus_handle;
        i2c_master_dev_handle_t dev_handle;

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
}