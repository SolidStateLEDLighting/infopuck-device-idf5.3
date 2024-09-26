#include "spi/spi_.hpp"

/* Local Semaphores */
SemaphoreHandle_t semSPIEntry = NULL;
SemaphoreHandle_t semSPIRouteLock = NULL;

/* External Semaphores */
extern SemaphoreHandle_t semSysEntry;

SPI::SPI(spi_host_device_t spiMyHost, int spiMyMOSIPin, int spiMyMISOPin, int spiMyClockPin)
{
    spiHost = spiMyHost;
    spiMOSIPin = spiMyMOSIPin;
    spiMISOPin = spiMyMISOPin;
    spiClockPin = spiMyClockPin;

    if (xSemaphoreTake(semSysEntry, portMAX_DELAY)) // Get everything from the system that we need.
    {
        if (sys == nullptr)
            sys = System::getInstance();
        taskHandleSystemRun = sys->getRunTaskHandle();
        xSemaphoreGive(semSysEntry);
    }

    setFlags();         // Enable logging statements for any area of concern.
    setLogLevels();     // Manually sets log levels for other tasks down the call stack.
    createSemaphores(); // Creates any locking semaphores owned by this object.
    createQueues();     // We use queues in several areas.
    // restoreVariablesFromNVS(); // Brings back all our persistant data.

    //
    // Set Object Debug Level
    // Note that this function can not raise log level above the level set using CONFIG_LOG_DEFAULT_LEVEL setting in menuconfig.
    esp_log_level_set(TAG, ESP_LOG_INFO);

    //
    //
    vSemaphoreCreateBinary(semSPIEntry); // External access semaphore
    if (semSPIEntry != NULL)
        xSemaphoreTake(semSPIEntry, portMAX_DELAY);
    //
    // Message Queues
    //
    xQueueSPICmdRequests = xQueueCreate(1, sizeof(SPI_CmdRequest *));
    ptrSPICmdReq = new SPI_CmdRequest();
    ptrSPICmdResp = new SPI_RESPONSE();
    //
    // Start our task and state machine
    //
    spiOP = SPI_OP::Init;
    initSPIStep = SPI_INIT::Start;

    logByValue(ESP_LOG_INFO, semSPIRouteLock, TAG, std::string(__func__) + "(): runStackSizeK: " + std::to_string(runStackSizeK));
    xTaskCreate(runMarshaller, "SPI::Run", 1024 * 3, this, runStackSizeK, &taskHandleRun);
}

SPI::~SPI()
{
    TaskHandle_t temp = this->taskHandleRun;
    this->taskHandleRun = nullptr;
    vTaskDelete(temp);

    if (xQueueSPICmdRequests != nullptr)
        vQueueDelete(xQueueSPICmdRequests);

    if (ptrSPICmdReq != NULL)
        delete (ptrSPICmdReq);

    if (ptrSPICmdResp != NULL)
        delete (ptrSPICmdResp);

    if (semSPIEntry != NULL)
        vSemaphoreDelete(semSPIEntry);
}

void SPI::setFlags()
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
    showSPI = 0;
    // showSPI |= _showSPI_SomeItem;
    // showSPI |= _showSPIShdnSteps
}

void SPI::setLogLevels()
{
    if ((show + showSPI) > 0)                 // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    //
    // To increase log information - set any or all the levels below at ESP_LOG_INFO, or ESP_LOG_DEBUG or ESP_LOG_VERBOSE
    //
}

void SPI::createSemaphores()
{
    semSPIEntry = xSemaphoreCreateBinary(); // External access semaphore
    if (semSPIEntry != NULL)
        xSemaphoreGive(semSPIEntry); // Initialize the Semaphore

    semSPIRouteLock = xSemaphoreCreateBinary();
    if (semSPIRouteLock != NULL)
        xSemaphoreGive(semSPIRouteLock);
}

void SPI::destroySemaphores()
{
    // Carefully match this list of actions against the one in createSemaphores()
    if (semSPIEntry != nullptr)
    {
        vSemaphoreDelete(semSPIEntry);
        semSPIEntry = nullptr;
    }

    if (semSPIRouteLock != nullptr)
    {
        vSemaphoreDelete(semSPIRouteLock);
        semSPIRouteLock = nullptr;
    }
}

void SPI::createQueues()
{
    esp_err_t ret = ESP_OK;

    if (queueCmdRequests == nullptr)
    {
        queueCmdRequests = xQueueCreate(1, sizeof(SPI_CmdRequest *)); // Initialize our Incoming Command Request Queue -- element is of size pointer
        ESP_GOTO_ON_FALSE(queueCmdRequests, ESP_ERR_NO_MEM, spi_createQueues_err, TAG, "IDF did not allocate memory for the command request queue.");
    }

    if (ptrSPICmdRequest == nullptr)
    {
        ptrSPICmdRequest = new SPI_CmdRequest();
        ESP_GOTO_ON_FALSE(ptrSPICmdRequest, ESP_ERR_NO_MEM, spi_createQueues_err, TAG, "IDF did not allocate memory for the ptrSPICmdRequest structure.");
    }
    return;

spi_createQueues_err:
    logByValue(ESP_LOG_ERROR, semSPIRouteLock, TAG, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void SPI::destroyQueues()
{
    if (queueCmdRequests != nullptr)
    {
        vQueueDelete(queueCmdRequests);
        queueCmdRequests = nullptr;
    }

    if (ptrSPICmdRequest != nullptr)
    {
        delete ptrSPICmdRequest;
        ptrSPICmdRequest = nullptr;
    }
}

/* Public Member Functions */
TaskHandle_t &SPI::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &SPI::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return xQueueSPICmdRequests;
}
