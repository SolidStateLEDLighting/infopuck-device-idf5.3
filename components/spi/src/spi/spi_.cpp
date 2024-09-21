#include "spi/spi_.hpp"

//
// External Varibles
//
SemaphoreHandle_t semSPIEntry = nullptr;

SPI::SPI(spi_host_device_t spiMyHost, int spiMyMOSIPin, int spiMyMISOPin, int spiMyClockPin)
{
    spiHost = spiMyHost;
    spiMOSIPin = spiMyMOSIPin;
    spiMISOPin = spiMyMISOPin;
    spiClockPin = spiMyClockPin;
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
    ptrSPICmdResp = new SPI_Response();
    //
    // Start our task and state machine
    //
    spiOP = SPI_OP::Init;
    initSPIStep = SPI_INIT::Start;
    xTaskCreate(runMarshaller, "SPI::Run", 1024 * 3, this, 8, &taskHandleRun);
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

/* Public Member Functions */
TaskHandle_t &SPI::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &SPI::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return xQueueSPICmdRequests;
}
