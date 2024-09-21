#include "imu/imu_.hpp";

void SPI::runMarshaller(void *arg)
{
    auto obj = (IMU *)arg;
    obj->run();

    if (obj->taskHandleIMU != nullptr)
        vTaskDelete(NULL);
}

void IMU::run(void) // I2C processing lives here for the lifetime of the object
{
    esp_err_t rc = ESP_OK;

    while (true) // Process all incomeing I2C requests and return any results
    {
        switch (imuOP)
        {
        case IMU_OP::Run:
        {
            if (xQueueReceive(xQueueIMUCmdRequests, &ptrIMUCmdReq, pdMS_TO_TICKS(1500))) // We wait here for each message
            {
            }

            if (showRun)
                ESP_LOGI(TAG, "IMU run");
            break;
        }

        case IMU_OP::Init: // Go through any slow initialization requirements here...
        {
            switch (initIMUStep)
            {
            case IMU_INIT::Start:
            {
                ESP_LOGI(TAG, "Initalization Start");
                initIMUStep = IMU_INIT::Load_NVS_Settings;
                [[fallthrough]];
            }

            case IMU_INIT::Load_NVS_Settings: // No settings to load at this time.
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 1  - Load_NVS_Settings");
                initIMUStep = IMU_INIT::Init_Bus;
                break;
            }

            case IMU_INIT::Finished:
            {
                ESP_LOGI(TAG, "Initialization Finished");
                xSemaphoreGive(semIMUEntry);
                break;
            }
            }
            break;
        }
        }
    }
}
