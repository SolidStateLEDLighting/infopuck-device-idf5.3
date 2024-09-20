#include "system_.hpp"

#include "driver/gpio.h"
#include "esp_check.h"

#include "esp_sleep.h"

//
// We generally handle GPIO interrupts here.  The idea is to route them to the handler which is designed for that service.
//
// We have one available tactile switch.  We can use this switch for debugging.  Notice that we debounce the
// switch in software.   We handle switch input with espressive's recommendation to first catching the ISR and then routing that
// to a queue.
//
/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semWifiEntry;
extern SemaphoreHandle_t semIndEntry;

bool allowSwitchGPIOinput = true; // These variables are used for switch input debouncing
uint8_t SwitchDebounceCounter = 5;

QueueHandle_t xQueueGPIOEvents = nullptr;

void System::initGPIOPins(void) // We initial all pins possible here.
{
    //
    // At one time, we were initializing all pins here, but new libraries have been taking some of that job away.
    //

    //
    // It may be important to disable all unused pins in a very specific ways to reduce power consumption.  This should be done here.
    //

    //
    // Tactile Switch(es)
    //
    gpio_config_t gpioSW1;             // We have one switch available in the S3 DevKits   GPIO_BOOT_SW
    gpioSW1.pin_bit_mask = 1LL << SW1; //
    gpioSW1.mode = GPIO_MODE_INPUT;    //
    gpioSW1.pull_up_en = GPIO_PULLUP_ENABLE;
    gpioSW1.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&gpioSW1);
}

/* This ISR is set apart because tactile switch input needs to be handled with a debouncing algorithm. */
void IRAM_ATTR GPIOSwitchIsrHandler(void *arg)
{
    if (allowSwitchGPIOinput)
    {
        // Important Note:  We are breaking the rules here by accessing variables in 2 different tasks without locking them.
        // In this particular case any errors that would result can not be seen.  We might have a skipped count or a
        // slightly longer delay.  This error would not matter.
        xQueueSendToBackFromISR(xQueueGPIOEvents, &arg, NULL);
        SwitchDebounceCounter = 5; // Reject all input for 5/10 of a second -- counter is running in system_timer
        allowSwitchGPIOinput = false;
    }
}

/* Normal GPIO ISR handling would warrant no debouncing delay. */
void IRAM_ATTR GPIOIsrHandler(void *arg)
{
    xQueueSendToBackFromISR(xQueueGPIOEvents, &arg, NULL);
}

void System::initGPIOTask(void)
{
    if (show & _showInit)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "()");

    esp_err_t ret = ESP_OK;

    xQueueGPIOEvents = xQueueCreate(1, sizeof(uint32_t)); // Create a queue to handle gpio events from isr
    ESP_GOTO_ON_FALSE(xQueueGPIOEvents, ESP_FAIL, sys_GPIOIsrHandler_err, TAG, "xQueueCreate() failed");

    ESP_GOTO_ON_ERROR(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT), sys_GPIOIsrHandler_err, TAG, "gpio_install_isr_service() failed");

    if (show & _showInit)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Started gpio isr service...");

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(SW1, GPIOSwitchIsrHandler, (void *)SW1), sys_GPIOIsrHandler_err, TAG, "gpio_isr_handler_add() failed");

    SwitchDebounceCounter = 10;
    allowSwitchGPIOinput = true;

    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): gpioStackSizeK: " + std::to_string(gpioStackSizeK));
    xTaskCreate(runGPIOTaskMarshaller, "sys_gpio", 1024 * gpioStackSizeK, this, TASK_PRIORITY_MID, &runTaskHandleSystemGPIO); // (1) Low number indicates low priority task
    return;

sys_GPIOIsrHandler_err:
    errMsg = std::string(__func__) + "(): " + std::string(esp_err_to_name(ret));
    sysOP = SYS_OP::Error;
}

void System::runGPIOTaskMarshaller(void *arg) // This function can be resolved at run time by the compiler.
{
    ((System *)arg)->runGPIOTask();
    ((System *)arg)->runTaskHandleSystemGPIO = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
    vTaskDelete(NULL);
}

void System::runGPIOTask(void)
{
    uint32_t io_num = 0;

    SYS_TEST_TYPE testType = SYS_TEST_TYPE::WIFI; // PICK YOUR STARTING TEST AREA
    uint8_t testIndex = 0;                        // SET YOUR STARTING TEST INDEX

    xQueueReset(xQueueGPIOEvents);

    while (true)
    {
        if (xQueueReceive(xQueueGPIOEvents, (void *)&io_num, portMAX_DELAY)) // There is never any reason to yield.
        {
            if (sysOP == SYS_OP::Init) // If we haven't finished out our initialization -- discard items from our queue.
                continue;

            switch (io_num)
            {
            case SW1:
            {
                routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): SW1 - testIndex = " + std::to_string(testIndex) + " Wakeup Cause = " + std::to_string((int)esp_sleep_get_wakeup_cause()));

                switch (testType)
                {
                case SYS_TEST_TYPE::IDLE:
                {
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): SYS_TEST_TYPE::IDLE...");
                    esp_pm_dump_locks(stdout);
                    break;
                }

                case SYS_TEST_TYPE::LIFE_CYCLE_CREATE:
                {
                    test_objectLifecycle_create(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::LIFE_CYCLE_DESTROY:
                {
                    test_objectLifecycle_destroy(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::POWER_MANAGEMENT:
                {
                    test_power_management(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::LIGHT_SLEEP:
                {
                    test_light_sleep(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::DEEP_SLEEP:
                {
                    test_deep_sleep(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::NVS:
                {
                    test_nvs(&testType, &testIndex);
                    break;
                }

                case SYS_TEST_TYPE::WIFI:
                {
                    test_wifi(&testType, &testIndex);
                    break;
                }
                }
                break;
            }

            default:
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Missing Case for io_num  " + std::to_string(io_num));
                break;
            }
        }
    }
}