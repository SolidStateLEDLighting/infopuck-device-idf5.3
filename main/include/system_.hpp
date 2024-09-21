#pragma once

#include "sdkconfig.h"     // Configuration variables
#include "system_defs.hpp" // Local definitions, structs, and enumerations

#include <stdio.h> // Standard libraries
#include <inttypes.h>
#include <stdbool.h>
#include <sstream>
#include <memory>

#include "freertos/task.h" // RTOS libraries (remaining files)
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOSConfig.h"

#include <driver/gpio.h> // IDF components
#include "esp_system.h"
#include <esp_log.h>
#include <esp_err.h>
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "nvs/nvs_.hpp"         // Our components
#include "display/display_.hpp" //
#include "i2c/i2c_.hpp"         //
#include "wifi/wifi_.hpp"       // NOTE: SNTP is completely isolated from the System

class NVS; // Forward declarations
class Display;
class I2C;
class Wifi;

extern "C"
{
    class System
    {
    public:
        static System *getInstance(esp_reset_reason_t resetReason = ESP_RST_UNKNOWN) // Enforce use of System as a singleton object
        {
            static System sysInstance = System(resetReason); // Upon a new System object, it may be important to respond to a reset in a unique way.
            return &sysInstance;
        }

        TaskHandle_t getRunTaskHandle(void);
        QueueHandle_t getCmdRequestQueue(void);

    private:
        System(esp_reset_reason_t);
        System(const System &) = delete;         // Disable copy constructor
        void operator=(System const &) = delete; // Disable assignment operator

        char TAG[5] = "_sys";

        /* Object States */
        WIFI_CONN_STATE sysWifiConnState = WIFI_CONN_STATE::WIFI_DISCONNECTED;

        /* Object References */
        NVS *nvs = nullptr;
        // Speaker *speak = nullptr;
        Display *disp = nullptr;
        I2C *i2c = nullptr;
        // I2S *i2s_0 = nullptr;
        // I2S *i2s_1 = nullptr;
        // IMU *imu = nullptr;
        // MIC *mic = nullptr;
        // Speaker *speak = nullptr;
        // SPI *spi = nullptr;
        // TOUCH *touch = nullptr;
        Wifi *wifi = nullptr;

        uint8_t show = 0;    // Flags
        uint8_t showSys = 0; //
        uint8_t diagSys = 0; //

        uint32_t bootCount = 0;

        QueueHandle_t queHandleWIFICmdRequest = nullptr;
        QueueHandle_t queHandleI2CCmdRequest = nullptr;

        void resetHandling(esp_reset_reason_t);
        void setFlags(void);
        void setLogLevels(void);
        void createSemaphores(void);
        void createQueues(void);

        /* System_Diagnostics */
        void runDiagnostics(void);
        void printRunTimeStats(void);
        void printMemoryStats(void);
        void printTaskInfo(void);

        /* System_gpio */
        uint8_t gpioStackSizeK = 5;                     // Default minimum size
        TaskHandle_t runTaskHandleSystemGPIO = nullptr; //

        void initGPIOPins(void);
        void initGPIOTask(void);
        static void runGPIOTaskMarshaller(void *);
        void runGPIOTask(void); // Handles GPIO Interrupts on Change Events

        /* System_gpio_test */
        uint32_t freqValue;
        esp_pm_lock_handle_t cpu_lock_handle = nullptr;
        esp_pm_lock_handle_t apb_lock_handle = nullptr;

        void test_objectLifecycle_create(SYS_TEST_TYPE *, uint8_t *);
        void test_objectLifecycle_destroy(SYS_TEST_TYPE *, uint8_t *);
        void test_power_management(SYS_TEST_TYPE *, uint8_t *);
        void test_light_sleep(SYS_TEST_TYPE *, uint8_t *);
        void test_deep_sleep(SYS_TEST_TYPE *, uint8_t *);
        void test_nvs(SYS_TEST_TYPE *, uint8_t *);
        void test_wifi(SYS_TEST_TYPE *, uint8_t *);

        /* System_Logging */
        std::string errMsg = "";
        void routeLogByRef(LOG_TYPE, std::string *);
        void routeLogByValue(LOG_TYPE, std::string);

        /* System_NVS */
        bool saveToNVSFlag = false;
        uint8_t saveToNVSDelaySecs = 0;

        void restoreVariablesFromNVS(void);
        void saveVariablesToNVS(void);

        /* System_Run */
        SYS_NOTIFY sysTaskNotifyValue = (SYS_NOTIFY)0;

        QueueHandle_t systemCmdRequestQue = nullptr; // Command Queue
        SYS_CmdRequest *ptrSYSCmdRequest = nullptr;
        SYS_Response *ptrSYSResponse = nullptr;
        std::string strCmdPayload = "";

        uint8_t runStackSizeK = 8;                  // Default minimum size
        TaskHandle_t taskHandleSystemRun = nullptr; //

        SYS_OP sysOP = SYS_OP::Idle; // State variables
        SYS_OP opSys_Return = SYS_OP::Idle;
        SYS_INIT sysInitStep = SYS_INIT::Finished;

        TaskHandle_t taskHandleSpeakerRun = nullptr; // RTOS
        TaskHandle_t taskHandleWifiRun = nullptr;
        TaskHandle_t taskHandleI2CRun = nullptr;
        TaskHandle_t taskHandleWIFIRun = nullptr;

        static void runMarshaller(void *); // Handles all System activites
        void run(void);                    //

        /* System_Timer */
        uint8_t rebootTimerSec = 0;
        uint8_t syncEventTimeOut_Counter = 0;
        esp_timer_handle_t handleTimer = nullptr;

        uint8_t timerStackSizeK = 4;                  // Default minimum size
        TaskHandle_t taskHandleRunSysTimer = nullptr; //

        void initSysTimerTask(void);                   //
        static void runSysTimerTaskMarshaller(void *); // Handles all Timer related events
        void runSysTimerTask(void);                    //
        static void sysTimerCallback(void *);          //

        void halfSecondActions(void);
        void oneSecondActions(void);
        void fiveSecondActions(void);
        void tenSecondActions(void);
        void oneMinuteActions(void);

        /* Utilities */
        const char *convertWifiStateToChars(uint8_t);
        std::string getDeviceID(void);

        void lockSetBool(bool *, bool);                      // Locking bool variables
        bool lockGetBool(bool *);                            //
        uint8_t lockGetUint8(uint8_t *);                     // Locking uint8_t variables
        void lockOrUint8(uint8_t *, uint8_t);                //
        void lockAndUint8(uint8_t *variable, uint8_t value); //
        void lockSetUint8(uint8_t *, uint8_t);               //
        uint8_t lockDecrementUint8(uint8_t *);               //
    };
}