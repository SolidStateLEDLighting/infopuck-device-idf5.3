#include "i2c/i2c_.hpp"

/* Local Semaphores */
SemaphoreHandle_t semI2CEntry = nullptr;
SemaphoreHandle_t semI2CRouteLock;

I2C::I2C()
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

    vSemaphoreCreateBinary(semI2CEntry);
    if (semI2CEntry != nullptr)
    {
        xSemaphoreTake(semI2CEntry, portMAX_DELAY); // We hold our entry semaphore until full initialization is complete
    }

    setFlags();         // Enable logging statements for any area of concern.
    setLogLevels();     // Manually sets log levels for other tasks down the call stack.
    createSemaphores(); // Creates any locking semaphores owned by this object.
    createQueues();     // We use queues in several areas.

    xSemaphoreTake(semI2CEntry, portMAX_DELAY); // Take our semaphore and thereby lock entry to this object during its initialization.

    //
    // Start our task and state machine
    //
    i2cOP = I2C_OP::Init;
    initI2CStep = I2C_INIT::Start;
    xTaskCreate(runMarshaller, "I2C::Run", 1024 * 3, this, 8, &taskHandleRun);
}

I2C::~I2C()
{
    // Process of destroying this object:
    // 1) Lock the object with its entry semaphore. (done by the caller)
    // 2) Send out notifications to the users of I2C that it is shutting down. (done by caller)
    // 3) Send a task notification to CMD_SHUT_DOWN. (Looks like we are sending it to ourselves here, but this is not so...)
    // 4) Watch the runTaskHandle and wait for it to clean up and then become nullptr.
    // 5) Clean up other resources created by calling task from the constructor.
    // 6) UnLock the entry semaphore.
    // 7) Destroy all semaphores and queues at the same time. These are created by calling task in constructor.
    // 8) Done.

    // The calling task can still send taskNotifications to the indication task!
    while (!xTaskNotify(taskHandleRun, static_cast<uint32_t>(I2C_NOTIFY::CMD_SHUT_DOWN), eSetValueWithoutOverwrite))
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the notification to be received.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    while (taskHandleRun != nullptr)
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the indication task handle to become null.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    i2c_driver_delete(i2c_port);

    xSemaphoreTake(semI2CEntry, portMAX_DELAY); 

    destroySemaphores();
    destroyQueues();
}

void I2C::setFlags()
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
    showI2C = 0;
    // showDisplay |= _showDisplay_SomeItem;
    // showDisplay |= _showDisplayShdnSteps
}

void I2C::setLogLevels()
{
    if ((show + showI2C) > 0)                 // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    // To increase log information - set any or all the levels below at ESP_LOG_INFO, or ESP_LOG_DEBUG or ESP_LOG_VERBOSE
}

void I2C::createSemaphores()
{
    semI2CEntry = xSemaphoreCreateBinary(); // External access semaphore
    if (semI2CEntry != NULL)
        xSemaphoreGive(semI2CEntry); // Initialize the Semaphore

    semI2CRouteLock = xSemaphoreCreateBinary();
    if (semI2CRouteLock != NULL)
        xSemaphoreGive(semI2CRouteLock);
}

void I2C::destroySemaphores()
{
    // Carefully match this list of actions against the one in createSemaphores()
    if (semI2CEntry != nullptr)
    {
        vSemaphoreDelete(semI2CEntry);
        semI2CEntry = nullptr;
    }

    if (semI2CRouteLock != nullptr)
    {
        vSemaphoreDelete(semI2CRouteLock);
        semI2CRouteLock = nullptr;
    }
}

void I2C::createQueues()
{
    esp_err_t ret = ESP_OK;

    if (queueCmdRequests == nullptr)
    {
        queueCmdRequests = xQueueCreate(1, sizeof(I2C_CmdRequest *)); // Initialize our Incoming Command Request Queue -- element is of size pointer
        ESP_GOTO_ON_FALSE(queueCmdRequests, ESP_ERR_NO_MEM, i2c_createQueues_err, TAG, "IDF did not allocate memory for the command request queue.");
    }

    if (ptrI2CCmdRequest == nullptr)
    {
        ptrI2CCmdRequest = new I2C_CmdRequest();
        ESP_GOTO_ON_FALSE(ptrI2CCmdRequest, ESP_ERR_NO_MEM, i2c_createQueues_err, TAG, "IDF did not allocate memory for the ptrI2CCmdRequest structure.");
    }

    if (ptrI2CCmdResponse == nullptr)
    {
        ptrI2CCmdResponse = new I2C_CmdResponse();
        ESP_GOTO_ON_FALSE(ptrI2CCmdResponse, ESP_ERR_NO_MEM, i2c_createQueues_err, TAG, "IDF did not allocate memory for the ptrI2CCmdResponse structure.");
    }
    return;

i2c_createQueues_err:
    logByValue(ESP_LOG_ERROR, semI2CRouteLock, TAG, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void I2C::destroyQueues()
{
    if (queueCmdRequests != nullptr)
    {
        vQueueDelete(queueCmdRequests);
        queueCmdRequests = nullptr;
    }

    if (ptrI2CCmdRequest != nullptr)
    {
        delete ptrI2CCmdRequest;
        ptrI2CCmdRequest = nullptr;
    }

    if (ptrI2CCmdResponse != nullptr)
    {
        delete ptrI2CCmdResponse;
        ptrI2CCmdResponse = nullptr;
    }
}

/* Public Member Functions */
TaskHandle_t &I2C::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &I2C::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return queueCmdRequests;
}
