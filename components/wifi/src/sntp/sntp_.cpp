#include "sntp/sntp_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

SNTP *ptrSNTPInternal = nullptr; // Hold a 'this' pointer for the SNTP event handlers.

/* Local Semaphores */
SemaphoreHandle_t semSNTPRouteLock = NULL;

/* Construction / Destruction */
SNTP::SNTP()
{
    ptrSNTPInternal = this; // We plan on removing this someday when all ESP event handlers can be passed a 'this' pointer during registration.

    setFlags();            // Enable logging statements for any area of concern.
    setLogLevels();            // Manually sets log levels for other tasks down the call stack.
    createSemaphores();        // Creates any locking semaphores owned by this object.
    createQueues();            // We use queues in several areas.
    restoreVariablesFromNVS(); // Brings back all our persistant data.
}

SNTP::~SNTP()
{
    destroySemaphores();
    destroyQueues();
}

void SNTP::setFlags()
{
    // show variable is system wide defined and this exposes for viewing any general processes.
    show = 0;
    show |= _showInit; // Sets this bit
    // show |= _showNVS;
    // show |= _showRun;
    // show |= _showEvents;
    // show |= _showJSONProcessing;
    // show |= _showDebugging;
    // show |= _showProcess;
    // show |= _showPayload;

    showSNTP = 0;
    // showSNTP |= _showSNTPConnSteps;
}

void SNTP::setLogLevels()
{
    if ((show + showSNTP) > 0)                // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    esp_log_level_set("sntp", ESP_LOG_ERROR);
}

void SNTP::createSemaphores()
{
    semSNTPRouteLock = xSemaphoreCreateBinary();
    if (semSNTPRouteLock != NULL)
        xSemaphoreGive(semSNTPRouteLock);
}

void SNTP::createQueues()
{
    esp_err_t ret = ESP_OK;

    queueEvents = xQueueCreate(2, sizeof(SNTP_Event)); // Initialize the queue that holds SNTP events
    ESP_GOTO_ON_FALSE(queueEvents, ESP_ERR_NO_MEM, wifi_createQueues_err, TAG, "IDF did not allocate memory for the events queue.");
    return;

wifi_createQueues_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void SNTP::destroySemaphores()
{
    if (semSNTPRouteLock != nullptr)
    {
        vSemaphoreDelete(semSNTPRouteLock);
        semSNTPRouteLock = nullptr;
    }
}

void SNTP::destroyQueues()
{
    if (queueEvents != nullptr)
    {
        vQueueDelete(queueEvents);
        queueEvents = nullptr;
    }
}