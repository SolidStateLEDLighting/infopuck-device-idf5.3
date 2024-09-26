#include "logging/logging_.hpp"
//
// I bring most logging formation here (inside each object) because in a more advanced project, I route logging
// information back to the cloud.  We could also just as easily log to a file storage location like an SD card.
//
// At is also at this location (in my more advanced projects) that I store Error information to Flash.  This makes it possible
// to transmit error logging to the cloud after a reboot.
//
void Logging::logByValue(esp_log_level_t level, SemaphoreHandle_t semRouteLock, char *ourTAG, std::string msg)
{
    if (xSemaphoreTake(semRouteLock, portMAX_DELAY)) // We use this lock to prevent sys_evt and disp_run tasks from having conflicts
    {
        switch (level)
        {
        case ESP_LOG_NONE:
        {
            break;
        }

        case ESP_LOG_ERROR:
        {
            ESP_LOGE(ourTAG, "%s", (msg).c_str()); // Print out our errors here so we see it in the console.
            break;
        }

        case ESP_LOG_WARN:
        {
            ESP_LOGW(ourTAG, "%s", (msg).c_str()); // Print out our warning here so we see it in the console.
            break;
        }

        case ESP_LOG_INFO:
        {
            ESP_LOGI(ourTAG, "%s", (msg).c_str()); // Print out our information here so we see it in the console.
            break;
        }

        case ESP_LOG_DEBUG:
        {
            break;
        }

        case ESP_LOG_VERBOSE:
        {
            break;
        }
        }

        xSemaphoreGive(semRouteLock);
    }
}

void Logging::logTaskInfo(SemaphoreHandle_t routingSemaphoreHandle, char *ourTAG)
{
    char *name = pcTaskGetName(NULL); // Note: The value of NULL can be used as a parameter if the statement is running on the task of your inquiry.
    uint32_t priority = uxTaskPriorityGet(NULL);
    uint32_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);

     logByValue(ESP_LOG_INFO, routingSemaphoreHandle, ourTAG, std::string(__func__) + "(): name: " + std::string(name) + " priority: " + std::to_string(priority) + " highWaterMark: " + std::to_string(highWaterMark));
}