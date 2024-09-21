#include "spi/spi_.hpp"

/* External Semaphores */
extern SemaphoreHandle_t semSPIEntry;

void SPI::runMarshaller(void *arg)
{
    auto obj = (SPI *)arg;
    obj->run();

    if (obj->taskHandleRun != nullptr)
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
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 1  - Start");

                //if (show & _showInit)
                    //routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): SPI_INIT::Start");
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
