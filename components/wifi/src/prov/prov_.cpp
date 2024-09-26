#include "prov/prov_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

PROV *ptrPROVInternal = nullptr; // Hold a 'this' pointer for the PROV event handlers.

/* Local Semaphores */
SemaphoreHandle_t semProvEntry;
SemaphoreHandle_t semProvRouteLock;

/* Construction / Destruction */
PROV::PROV(QueueHandle_t queueCmdRequests)
{
    ptrPROVInternal = this; // We plan on removing this someday when all ESP event handlers can be passed a 'this' pointer during registration.

    wifiQueCmdRequests = queueCmdRequests; // Received a wifi Command Request Queue handle

    setFlags();                    // Enable logging statements for any area of concern.
    setLogLevels();                // Manually sets log levels for other tasks down the call stack.
    setConditionalCompVariables(); //
    createSemaphores();            // Creates any locking semaphores owned by this object.
    createQueues();                // We use queues in several areas.

    if (semProvEntry)                                // Outsiders will not be able to gain entry until provisioning is complete.
        xSemaphoreTake(semProvEntry, portMAX_DELAY); // We use this as a way make the calling object wait.

    provRunStep = PROV_RUN::Start; // By default, we immediately enter into a provision action.
    provOP = PROV_OP::Run;

    xTaskCreate(runMarshaller, "prov_run", 1024 * 6, this, TASK_PRIORITY_MID, &taskHandleRun);
}

PROV::~PROV()
{
    // It is possible that a calling object may want to cancel the process.  We allow for that here.
    //
    // Process of destroying this object:
    // 1) Set blnCancelProvisioning = true;
    // 2) Watch the runTaskHandle and wait for them to clean up and then become nullptr.
    // 3) Clean up other resources created by calling task.
    // 4) UnLock the its entry semaphore.
    // 5) Destroy all semaphores and queues at the same time. These are created by calling task in the constructor.
    // 6) Done.

    // NOTE: This is a cross-task (thread) call, but it is not very dangerous.
    blnCancelProvisioning = true; // We may or may-not be in the process of provisioning.  It doesn't matter.

    while (taskHandleRun != nullptr)
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the provisioning task handle to become null.
    taskYIELD();                       // One last yield to make sure Idle task can run.

    xSemaphoreGive(semProvEntry); // Remember, this is the calling task which calls to "Give"
    destroySemaphores();
    destroyQueues();
}

void PROV::setFlags()
{
    // show variable is system wide defined and this exposes for viewing any general processes.
    show = 0;
    // show |= _showInit; // Sets this bit
    // show |= _showNVS;
    // show |= _showRun;
    // show |= _showEvents;
    // show |= _showJSONProcessing;
    // show |= _showDebugging;
    // show |= _showProcess;
    // show |= _showPayload;

    showPROV = 0;
    showPROV |= _showProvRun;
}

void PROV::setLogLevels()
{
    if ((show + showPROV) > 0)                // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.
}

void PROV::setConditionalCompVariables()
{
    // Our goal here is to keep the project very clean and neat.  Conditional complilation is horrible for formatting, so
    // we will convert all statements to neatly managed variables.  Everything here should be listed in Kconfig order.
    // The down-side is that our compiled code is larger and perhaps a bit slower.

#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
    blnSecurityVersion1 = true;
    blnSecurityVersion2 = false;
#elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
    blnSecurityVersion1 = false;
    blnSecurityVersion2 = true;
#endif

#ifdef CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    blnSec2DevelopmentMode = true;
#endif

#ifdef CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    blnSec2ProductionMode = true;
#endif

#ifdef CONFIG_EXAMPLE_RESET_PROVISIONED
    blnResetProvisioning = true;
#endif

#ifdef CONFIG_EXAMPLE_PROV_SEC2_USERNAME
    username = EXAMPLE_PROV_SEC2_USERNAME;
#endif

#ifdef CONFIG_EXAMPLE_PROV_SEC2_PWD
    pop = const char *pop = EXAMPLE_PROV_SEC2_PWD;
#endif

#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
    blnResetProvMgrOnFailure = true;
#endif

#ifdef CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT
    maxFailureRetryCount = CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT;
#endif

#ifdef CONFIG_EXAMPLE_REPROVISIONING
    // If reprovisioning is allowed, the auto-stop feature will be disabled.  Under normal conditions you will not want this unless you can reprovision more than one station connection at a time.
    blnAllowReprovisioning = true;
#endif
}

void PROV::createSemaphores()
{
    semProvEntry = xSemaphoreCreateBinary(); // External access semaphore
    if (semProvEntry != NULL)
        xSemaphoreGive(semProvEntry); // Initialize the Semaphore

    semProvRouteLock = xSemaphoreCreateBinary();
    if (semProvRouteLock != NULL)
        xSemaphoreGive(semProvRouteLock);
}

void PROV::destroySemaphores()
{
    // Carefully match this list of actions against the one in createSemaphores()
    if (semProvEntry != nullptr)
    {
        vSemaphoreDelete(semProvEntry);
        semProvEntry = nullptr;
    }

    if (semProvRouteLock != nullptr)
    {
        vSemaphoreDelete(semProvRouteLock);
        semProvRouteLock = nullptr;
    }
}

void PROV::createQueues()
{
    esp_err_t ret = ESP_OK;

    queueEvents = xQueueCreate(2, sizeof(PROV_Event)); // Initialize the queue that holds provision events
    ESP_GOTO_ON_FALSE(queueEvents, ESP_ERR_NO_MEM, prov_createQueues_err, TAG, "IDF did not allocate memory for the events queue.");
    return;

prov_createQueues_err:
    logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void PROV::destroyQueues()
{
    if (queueEvents != nullptr)
    {
        vQueueDelete(queueEvents);
        queueEvents = nullptr;
    }
}

/* Public Member Functions */
