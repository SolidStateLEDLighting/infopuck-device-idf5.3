#include "sntp/sntp_.hpp"
#include "nvs/nvs_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semSNTPRouteLock;

/* NVS */
void SNTP::restoreVariablesFromNVS()
{
    esp_err_t ret = ESP_OK;
    bool successFlag = true;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("sntp"), sntp_restoreVariablesFromNVS_err, TAG, "nvs->openNVSStorage('sntp') failed");

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): sntp namespace start");

    if (successFlag) // Restore serverIndex
    {
        ret = nvs->readU8IntegerFromNVS("serverIndex", &serverIndex);

        if (ret == ESP_OK)
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): serverIndex         is " + std::to_string(serverIndex));
        }
        else
        {
            successFlag = false;
            logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Error, Unable to restore serverIndex");
        }
    }

    if (successFlag) // Restore sntpTimeZone
    {
        // Grab a snapshot of the default value.  Typically this is accurate during startup as restoreVariablesFromNVS() is called almost never during normal operation.
        std::string holder = timeZone;

        if (nvs->readStringFromNVS("timeZone", &timeZone) == ESP_OK)
        {
            if (timeZone.length() < 1) // Do not allow empty strings for thie variable in nvs.
            {
                timeZone = holder;                                  // If the stored variable is of zero length,
                ret = nvs->writeStringToNVS("timeZone", &timeZone); // save the default value to nvs.
            }

            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): timeZone            is " + timeZone); // Confirm our restored value
        }

        if (ret != ESP_OK)
        {
            successFlag = false;
            logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Error, Unable to restore timeZone");
        }
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): sntp namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): Success");
    }
    else
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

sntp_restoreVariablesFromNVS_err:
    logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}

void SNTP::saveVariablesToNVS()
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
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("sntp"), sntp_saveVariablesToNVS_err, TAG, "nvs->openNVSStorage('sntp') failed");

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): sntp namespace start");

    if (successFlag) // Save serverIndex
    {
        if (nvs->writeU8IntegerToNVS("serverIndex", serverIndex) == ESP_OK)
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): serverIndex         = " + std::to_string(serverIndex));
        }
        else
        {
            successFlag = false;
            logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Unable to save serverIndex");
        }
    }

    if (successFlag) // Save timeZone
    {
        if (nvs->writeStringToNVS("timeZone", &timeZone) == ESP_OK)
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): timeZone            = " + timeZone);
        }
        else
        {
            successFlag = false;
            logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Unable to save timeZone");
        }
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): sntp namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): Success");
    }
    else
        logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

sntp_saveVariablesToNVS_err:
    logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}
