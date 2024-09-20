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
    xTaskCreate(runMarshaller, "SPI::Run", 1024 * 3, this, 8, &taskHandleSPI);
}

SPI::~SPI()
{
    TaskHandle_t temp = this->taskHandleSPI;
    this->taskHandleSPI = nullptr;
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

QueueHandle_t &SPI::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return xQueueSPICmdRequests;
}

void SPI::runMarshaller(void *arg)
{
    auto obj = (SPI *)arg;
    obj->run();

    if (obj->taskHandleSPI != nullptr)
        vTaskDelete(NULL);
}

void SPI::run(void) // I2C processing lives here for the lifetime of the object
{
    esp_err_t rc = ESP_OK;

    while (true) // Process all incomeing I2C requests and return any results
    {
        switch (spiOP)
        {
        case SPI_OP::Run:
        {
            if (xQueueReceive(xQueueSPICmdRequests, &ptrSPICmdReq, pdMS_TO_TICKS(1500))) // We wait here for each message
            {
            }

            if (showRun)
                ESP_LOGI(TAG, "SPI run");
            break;
        }

        case SPI_OP::Init: // Go through any slow initialization requirements here...
        {
            switch (initSPIStep)
            {
            case SPI_INIT::Start:
            {
                ESP_LOGI(TAG, "Initalization Start");
                initSPIStep = SPI_INIT::Load_NVS_Settings;
                [[fallthrough]];
            }

            case SPI_INIT::Load_NVS_Settings: // No settings to load at this time.
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 1  - Load_NVS_Settings");
                initSPIStep = SPI_INIT::Init_Bus;
                break;
            }

            case SPI_INIT::Init_Bus:
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 2  - Init_Bus");

                spi_bus_config_t buscfg = {
                    .mosi_io_num = spiMOSIPin,
                    .miso_io_num = spiMISOPin,
                    .sclk_io_num = spiClockPin,
                    .data2_io_num = -1,
                    .data3_io_num = -1,
                    .data4_io_num = -1,
                    .data5_io_num = -1,
                    .data6_io_num = -1,
                    .data7_io_num = -1,
                    .max_transfer_sz = 32,
                    .flags = 0,
                    .intr_flags = 0,
                };

                rc = spi_bus_initialize(spiHost, &buscfg, SPI_DMA_DISABLED);
                ESP_ERROR_CHECK(rc);

                initSPIStep = SPI_INIT::Finished;
                break;
            }

            case SPI_INIT::Finished:
            {
                ESP_LOGI(TAG, "Initialization Finished");
                spiOP = SPI_OP::Run;
                xSemaphoreGive(semSPIEntry);
                break;
            }
            }
            break;
        }
        }
    }
}
