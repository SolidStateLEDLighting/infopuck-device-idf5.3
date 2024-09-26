#include "wifi/wifi_.hpp"
#include "system_.hpp" // Class structure and variables

#include "freertos/task.h" // RTOS libraries

/* Local Semaphores */
SemaphoreHandle_t semWifiEntry = NULL;
SemaphoreHandle_t semWifiRouteLock = NULL;

/* External Semaphores */
extern SemaphoreHandle_t semSysEntry;

/* Construction / Destruction */
Wifi::Wifi()
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

    // The SNTP services are required all the time when WIFI is connected.  For that reason, we don't spin a new task for SNTP.  Instead
    // we increase the task memory in the Wifi to accommodate SNTP.  In contrast to Provision where we will allow the Provision object to create
    // its own task because we don't want to inflate the Wifi task memory to include Provision when that object is only used occasionally.
    // When Provision is destroyed, (or never created) that task memory is conserved.
    if (sntp == nullptr)
        sntp = new SNTP();

    setFlags();                // Enable logging statements for any area of concern.
    setLogLevels();            // Manually sets log levels for other tasks down the call stack.
    createSemaphores();        // Creates any locking semaphores owned by this object.
    createQueues();            // We use queues in several areas.
    restoreVariablesFromNVS(); // Brings back all our persistant data.

    xSemaphoreTake(semWifiEntry, portMAX_DELAY); // Take our semaphore and thereby lock entry to this object during its initialization.

    wifiInitStep = WIFI_INIT::Start; // Allow the object to initialize.  This takes some time.
    wifiOP = WIFI_OP::Init;

    logByValue(ESP_LOG_INFO, semWifiRouteLock, TAG, std::string(__func__) + "(): runStackSizeK: " + std::to_string(runStackSizeK));
    xTaskCreate(runMarshaller, "wifi_run", 1024 * runStackSizeK, this, TASK_PRIORITY_MID, &taskHandleWIFIRun);
}

Wifi::~Wifi()
{
    // Process of destroying this object:
    // 1) Lock the object with its entry semaphore. (done by the caller)
    // 2) Send out notifications to the users of Wifi that it is shutting down. (done by caller)
    // 3) Send a task notification to CMD_SHUT_DOWN. (Looks like we are sending it to ourselves here, but this is not so...)
    // 4) Watch the runTaskHandle (and any other possible task handles) and wait for them to clean up and then become nullptr.
    // 5) Clean up other resources created by calling task.
    // 6) UnLock the its entry semaphore.
    // 7) Destroy all semaphores and queues at the same time. These are created by calling task in the constructor.
    // 8) Done.

    // NOTE: The calling task can still send taskNotifications to the wifi task!
    while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_SHUT_DOWN), eSetValueWithoutOverwrite))
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the notification to be received.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    while (taskHandleWIFIRun != nullptr)
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the wifi task handle to become null.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    if (sntp != nullptr) // Destroy sntp
        delete sntp;     // Has no active tasks so it is simple to destroy

    xSemaphoreGive(semWifiEntry); // Remember, this is the calling task which calls to "Give"
    destroySemaphores();
    destroyQueues();
}

void Wifi::setFlags()
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

    // showWifi exposes wifi sub-processes.
    showWifi = 0;
    showWifi |= _showWifiDirectiveSteps;
    showWifi |= _showWifiConnSteps;
    showWifi |= _showWifiDiscSteps;
    showWifi |= _showWifiProvSteps;
    // showWifi |= _showWifiShdnSteps;
}

void Wifi::setLogLevels()
{
    if ((show + showWifi) > 0)                // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    //
    // To increase log information - set any or all the levels below at ESP_LOG_INFO, or ESP_LOG_DEBUG or ESP_LOG_VERBOSE
    //

    // We currently define our TAG "_wifi", but there is another TAG definition below ours in the IDF library that is also called "wifi".
    esp_log_level_set("wifi", ESP_LOG_ERROR);

    esp_log_level_set("esp-tls", ESP_LOG_ERROR);   // All these TAGs will reveal other information if the level is lowered.  Comment these out
    esp_log_level_set("wifi_init", ESP_LOG_ERROR); // to view any default log information that is normally generated.
    esp_log_level_set("phy_init", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_lwip", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
}

void Wifi::createSemaphores()
{
    semWifiEntry = xSemaphoreCreateBinary(); // External access semaphore
    if (semWifiEntry != NULL)
        xSemaphoreGive(semWifiEntry); // Initialize the Semaphore

    semWifiRouteLock = xSemaphoreCreateBinary();
    if (semWifiRouteLock != NULL)
        xSemaphoreGive(semWifiRouteLock);
}

void Wifi::destroySemaphores()
{
    // Carefully match this list of actions against the one in createSemaphores()
    if (semWifiEntry != nullptr)
    {
        vSemaphoreDelete(semWifiEntry);
        semWifiEntry = nullptr;
    }

    if (semWifiRouteLock != nullptr)
    {
        vSemaphoreDelete(semWifiRouteLock);
        semWifiRouteLock = nullptr;
    }
}

void Wifi::createQueues()
{
    esp_err_t ret = ESP_OK;

    if (queueEvents == nullptr)
    {
        queueEvents = xQueueCreate(5, sizeof(WIFI_Event)); // Initialize the queue that holds Wifi events -- element is of size WIFI_Event
        ESP_GOTO_ON_FALSE(queueEvents, ESP_ERR_NO_MEM, wifi_createQueues_err, TAG, "IDF did not allocate memory for the events queue.");
    }

    if (queueCmdRequests == nullptr)
    {
        queueCmdRequests = xQueueCreate(1, sizeof(WIFI_CmdRequest *)); // Initialize our Incoming Command Request Queue -- element is of size pointer
        ESP_GOTO_ON_FALSE(queueCmdRequests, ESP_ERR_NO_MEM, wifi_createQueues_err, TAG, "IDF did not allocate memory for the command request queue.");
    }

    if (ptrWifiCmdRequest == nullptr)
    {
        ptrWifiCmdRequest = new WIFI_CmdRequest();
        ESP_GOTO_ON_FALSE(ptrWifiCmdRequest, ESP_ERR_NO_MEM, wifi_createQueues_err, TAG, "IDF did not allocate memory for the ptrWifiCmdRequest structure.");
    }
    return;

wifi_createQueues_err:
    logByValue(ESP_LOG_ERROR, semWifiRouteLock, TAG, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void Wifi::destroyQueues()
{
    if (queueEvents != nullptr)
    {
        vQueueDelete(queueEvents);
        queueEvents = nullptr;
    }

    if (queueCmdRequests != nullptr)
    {
        vQueueDelete(queueCmdRequests);
        queueCmdRequests = nullptr;
    }

    if (ptrWifiCmdRequest != nullptr)
    {
        delete ptrWifiCmdRequest;
        ptrWifiCmdRequest = nullptr;
    }
}

/* Public Member Functions */
TaskHandle_t &Wifi::getRunTaskHandle(void)
{
    return taskHandleWIFIRun;
}

QueueHandle_t &Wifi::getCmdRequestQueue()
{
    return queueCmdRequests;
}
