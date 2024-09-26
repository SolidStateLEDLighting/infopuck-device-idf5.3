#include "system_.hpp"

#include "esp_system.h"
#include "esp_heap_caps.h"

/* External Semaphores */
extern SemaphoreHandle_t semSysRouteLock;

/* External Variables */
extern bool allowSwitchGPIOinput;
extern uint8_t SwitchDebounceCounter;

// Local Variables
uint8_t oneSecondCount = 10;
uint8_t fiveSecondCount = 5;
uint8_t tenSecondCount = 10;
uint8_t oneMinuteCount = 6;

//
// The timer here is of good precision but because all control to other objects is done through freeRTOS mechanisms, they would not
// be best for short time intervals.  So, we refrain from calling other objects for short time periods.
//
void System::initSysTimerTask(void)
{
    logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): timerStackSizeK: " + std::to_string(timerStackSizeK));
    xTaskCreate(runSysTimerTaskMarshaller, "sys_tmr", 1024 * timerStackSizeK, this, TASK_PRIORITY_HIGH, &taskHandleRunSysTimer);

    const esp_timer_create_args_t general_timer_args = {
        &System::sysTimerCallback, //
        this,                      // Argument to pass to the callback
        ESP_TIMER_TASK,            //
        "system_timer",            // Name is optional, but may help identify the timer when debugging
        true,                      // Skip unhandled events for periodic timers
    };

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(esp_timer_create(&general_timer_args, &handleTimer), sys_initSysTimer_err, TAG, "esp_timer_create() failed");
    ESP_GOTO_ON_ERROR(esp_timer_start_periodic(handleTimer, TIMER_PERIOD_10Hz), sys_initSysTimer_err, TAG, "esp_timer_create() failed");
    return;

sys_initSysTimer_err:
    errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
    sysOP = SYS_OP::Error;
}

void IRAM_ATTR System::sysTimerCallback(void *arg)
{
    // Our priorty here is to exit our ISR with minumum processing so the system can return to normal task servicing quickly.
    // Therefore, we will exercise Deferred Interrupt Processing as often as posible.
    // NOTE: Any high priorty task will essentially run as if it were the ISR itself if there are no other equally high prioirty tasks running.
    //
    vTaskNotifyGiveFromISR(((System *)arg)->taskHandleRunSysTimer, NULL);
}

void System::runSysTimerTaskMarshaller(void *arg)
{
    ((System *)arg)->runSysTimerTask();
    ((System *)arg)->taskHandleRunSysTimer = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
    vTaskDelete(NULL);
}

void System::runSysTimerTask(void)
{
    // uint32_t startRunTicks = xTaskGetTickCount();
    // uint32_t Hz = 0;

    while (true)
    {
        /* // Sanity Test for the system_timer
        if (pdTICKS_TO_MS(xTaskGetTickCount() - startRunTicks) > 1000) // Once second has just passed...
        {
            ESP_LOGW(TAG, "Running at %ld Hz", Hz); // Report cycles per second
            startRunTicks = xTaskGetTickCount();    // Restart the count
            Hz = 0;                                 //
        }
        else
            Hz++; */

        //
        // 10hz Processing
        //
        // We are using a generic task notification to trigger this routine at 10hz.  This command blocks until a
        // notification arrives.  We don't examine any notification value.  The value is ignored.
        //
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (SwitchDebounceCounter > 0) // Perform GPIO Switch Debouncing delay here.
        {
            // Important Note:  We are breaking the rules here by accessing variables in 2 different tasks without locking them.
            // In this particular case any errors that would result can not be seen.  We might have a skipped count or a
            // slightly longer delay.  The error would not matter.
            if (--SwitchDebounceCounter < 1)
                allowSwitchGPIOinput = true;
        }

        if (oneSecondCount > 0)
        {
            if (--oneSecondCount < 1)
            {
                oneSecondActions();
                oneSecondCount = 10;

                if (fiveSecondCount > 0)
                {
                    if (--fiveSecondCount < 1)
                    {
                        fiveSecondActions();
                        fiveSecondCount = 5;
                    }
                }

                if (tenSecondCount > 0)
                {
                    if (--tenSecondCount < 1)
                    {
                        tenSecondActions();
                        tenSecondCount = 10;
                    }
                }

                if (oneMinuteCount > 0)
                {
                    if (--oneMinuteCount < 1)
                    {
                        oneMinuteActions();
                        oneMinuteCount = 60;
                    }
                }
            }
        }
    }
}

/* Periodic Actions */
void System::halfSecondActions(void)
{
}

void System::oneSecondActions(void)
{
    if (showSys & _showSysTimerSeconds)
        logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): One Second");

    //
    // When we are working with multiple variables at the same time, we don't want 'save to NVS' being called too quickly.
    // Allow a save even if we are in the process of reboot count-down.
    //
    if (lockGetUint8(&saveToNVSDelaySecs) > 0)
    {
        if (lockDecrementUint8(&saveToNVSDelaySecs) < 1)
            lockSetBool(&saveToNVSFlag, true);
    }

    /* Reboot Request */
    if (rebootTimerSec > 0) // Currently, in this project, we don't invoke a reboot, but we will someday.
    {
        logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): Reboot in " + std::to_string(rebootTimerSec));
        if (--rebootTimerSec < 1)
            esp_restart(); // Reboot
        else
            return; // Don't permit further entry into the oneSecondActions routine while waiting for a reboot to begin.
    }
}

void System::fiveSecondActions(void)
{
    if (showSys & _showSysTimerSeconds)
        logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): Five Seconds");
}

void System::tenSecondActions(void)
{
    if (showSys & _showSysTimerSeconds)
        logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): Ten Seconds");

    lockOrUint8(&diagSys, _diagHeapCheck); // Set the diag bit to run the heap_caps_check_integrity_all(true) test
}

void System::oneMinuteActions(void)
{
    if (showSys & _showSysTimerMinutes)
        logByValue(ESP_LOG_INFO, semSysRouteLock, TAG, std::string(__func__) + "(): One Minute");
}