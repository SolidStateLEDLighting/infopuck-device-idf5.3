
#include "display/display_.hpp"
#include "system_.hpp" // Class structure and variables

#include "freertos/task.h" // RTOS libraries

/* Local Semaphores */
SemaphoreHandle_t semDisplayEntry = NULL;
SemaphoreHandle_t semDisplayRouteLock = NULL;

/* External Semaphores */
extern SemaphoreHandle_t semSysEntry;

/* Construction / Destruction */
Display::Display()
{
    // Process of creating this object:
    // 1) Get the system run task handle
    // 2) Create the SNTP object.
    // 3) Set the show flags.
    // 4) Set log levels
    // 5) Create all the RTOS resources
    // 6) Restore all the object variables from nvs.
    // 7) Lock the object with its entry semaphore.
    // 8) Start this object's run task.
    // 9) Done.

    // NOTE: Becoming RAII compliant may not be terribly interesting until the Esp32 is running asymmetric multiprocessing.

    if (xSemaphoreTake(semSysEntry, portMAX_DELAY)) // Get everything from the system that we need.
    {
        if (sys == nullptr)
            sys = System::getInstance();
        taskHandleSystemRun = sys->getRunTaskHandle();
        xSemaphoreGive(semSysEntry);
    }

    setFlags();                // Enable logging statements for any area of concern.
    setLogLevels();            // Manually sets log levels for other tasks down the call stack.
    createSemaphores();        // Creates any locking semaphores owned by this object.
    createQueues();            // We use queues in several areas.
    restoreVariablesFromNVS(); // Brings back all our persistant data.

    xSemaphoreTake(semDisplayEntry, portMAX_DELAY); // Take our semaphore and thereby lock entry to this object during its initialization.

    dispInitStep = DISPLAY_INIT::Start; // Allow the object to initialize.  This takes some time.
    dispOP = DISPLAY_OP::Init;

    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizek: " + std::to_string(runStackSizeK));
    xTaskCreate(runMarshaller, "disp_run", 1024 * runStackSizeK, this, TASK_PRIORITY_MID, &taskHandleRun);
}

Display::~Display()
{
    // Process of destroying this object:
    // 1) Lock the object with its entry semaphore. (done by the caller)
    // 2) Send out notifications to the users of Display that it is shutting down. (done by caller)
    // 3) Send a task notification to CMD_SHUT_DOWN. (Looks like we are sending it to ourselves here, but this is not so...)
    // 4) Watch the runTaskHandle (and any other possible task handles) and wait for them to clean up and then become nullptr.
    // 5) Clean up other resources created by calling task.
    // 6) UnLock the its entry semaphore.
    // 7) Destroy all semaphores and queues at the same time. These are created by calling task in the constructor.
    // 8) Done.

    // NOTE: The calling task can still send taskNotifications to the display task!  NOTE: We are accessing the task handle
    // in an unsafe way.
    while (!xTaskNotify(taskHandleRun, static_cast<uint32_t>(DISPLAY_NOTIFY::CMD_SHUT_DOWN), eSetValueWithoutOverwrite))
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the notification to be received.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    while (taskHandleRun != nullptr)
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the display task handle to become null.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    xSemaphoreGive(semDisplayEntry); // Remember, this is the calling task which calls to "Give"
    destroySemaphores();
    destroyQueues();
}

void Display::setFlags()
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

    // showDisplay exposes Display sub-processes.
    showDisplay = 0;
    // showDisplay |= _showDisplay_SomeItem;
    // showDisplay |= _showDisplayShdnSteps
}

void Display::setLogLevels()
{
    if ((show + showDisplay) > 0)                // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    //
    // To increase log information - set any or all the levels below at ESP_LOG_INFO, or ESP_LOG_DEBUG or ESP_LOG_VERBOSE
    //
}

void Display::createSemaphores()
{
    semDisplayEntry = xSemaphoreCreateBinary(); // External access semaphore
    if (semDisplayEntry != NULL)
        xSemaphoreGive(semDisplayEntry); // Initialize the Semaphore

    semDisplayRouteLock = xSemaphoreCreateBinary();
    if (semDisplayRouteLock != NULL)
        xSemaphoreGive(semDisplayRouteLock);
}

void Display::destroySemaphores()
{
    // Carefully match this list of actions against the one in createSemaphores()
    if (semDisplayEntry != nullptr)
    {
        vSemaphoreDelete(semDisplayEntry);
        semDisplayEntry = nullptr;
    }

    if (semDisplayRouteLock != nullptr)
    {
        vSemaphoreDelete(semDisplayRouteLock);
        semDisplayRouteLock = nullptr;
    }
}

void Display::createQueues()
{
    esp_err_t ret = ESP_OK;

    if (queueCmdRequests == nullptr)
    {
        queueCmdRequests = xQueueCreate(1, sizeof(DISPLAY_CmdRequest *)); // Initialize our Incoming Command Request Queue -- element is of size pointer
        ESP_GOTO_ON_FALSE(queueCmdRequests, ESP_ERR_NO_MEM, display_createQueues_err, TAG, "IDF did not allocate memory for the command request queue.");
    }

    if (ptrDisplayCmdRequest == nullptr)
    {
        ptrDisplayCmdRequest = new DISPLAY_CmdRequest();
        ESP_GOTO_ON_FALSE(ptrDisplayCmdRequest, ESP_ERR_NO_MEM, display_createQueues_err, TAG, "IDF did not allocate memory for the ptrDisplayCmdRequest structure.");
    }
    return;

display_createQueues_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void Display::destroyQueues()
{
    if (queueCmdRequests != nullptr)
    {
        vQueueDelete(queueCmdRequests);
        queueCmdRequests = nullptr;
    }

    if (ptrDisplayCmdRequest != nullptr)
    {
        delete ptrDisplayCmdRequest;
        ptrDisplayCmdRequest = nullptr;
    }
}

/* Public Member Functions */
TaskHandle_t &Display::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &Display::getCmdRequestQueue()
{
    return queueCmdRequests;
}
