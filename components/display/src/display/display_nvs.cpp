#include "display/display_.hpp"
#include "nvs/nvs_.hpp"
#include "system_.hpp" // Class structure and variables

/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semDisplayRouteLock;

/* NVS */
void Display::restoreVariablesFromNVS()
{
    esp_err_t ret = ESP_OK;
    bool successFlag = true;
    uint8_t temp = 0;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("display"), display_restoreVariablesFromNVS_err, TAG, "nvs->openNVSStorage('display') failed");

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): display namespace start");

    if (successFlag) // Restore runStackSizeK
    {
        temp = runStackSizeK;
        ret = nvs->readU8IntegerFromNVS("runStackSizeK", &temp); // This will save the default size if that value doesn't exist yet in nvs.

        if (ret == ESP_OK)
        {
            if (temp > runStackSizeK) // Ok to use any value greater than the default size.
            {
                runStackSizeK = temp;
                ret = nvs->writeU8IntegerToNVS("runStackSizeK", runStackSizeK); // Over-write the value with the default minumum value.
            }

            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): runStackSizeK       is " + std::to_string(runStackSizeK));
        }

        if (ret != ESP_OK)
        {
            logByValue(ESP_LOG_ERROR, semDisplayRouteLock, TAG, std::string(__func__) + "(): Error, Unable to restore runStackSizeK");
            successFlag = false;
        }
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): display namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): Success");
    }
    else
        logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

display_restoreVariablesFromNVS_err:
    logByValue(ESP_LOG_ERROR, semDisplayRouteLock, TAG, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}

void Display::saveVariablesToNVS()
{
    //
    // The best idea is to save only changed values.  Right now, we try to save everything.
    // The NVS object we call will avoid over-writing variables which already hold the identical values.
    // Later, we will add 'dirty' bits to avoid trying to save a value that hasn't changed.
    //
    esp_err_t ret = ESP_OK;
    bool successFlag = true;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("display"), display_saveVariablesToNVS_err, TAG, "nvs->openNVSStorage('display') failed");

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): display namespace start");

    if (successFlag) // Save runStackSizeK
    {
        if (nvs->writeU8IntegerToNVS("runStackSizeK", runStackSizeK) == ESP_OK)
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): runStackSizeK       = " + std::to_string(runStackSizeK));
        }
        else
        {
            successFlag = false;
            logByValue(ESP_LOG_ERROR, semDisplayRouteLock, TAG, std::string(__func__) + "(): Unable to writeU8IntegerToNVS runStackSizeK");
        }
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): display namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            logByValue(ESP_LOG_INFO, semDisplayRouteLock, TAG, std::string(__func__) + "(): Success");
    }
    else
        logByValue(ESP_LOG_ERROR, semDisplayRouteLock, TAG, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

display_saveVariablesToNVS_err:
    logByValue(ESP_LOG_ERROR, semDisplayRouteLock, TAG, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}
