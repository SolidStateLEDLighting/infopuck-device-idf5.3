#include "wifi/wifi_.hpp"
#include "nvs/nvs_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;

/* NVS */
void Wifi::restoreVariablesFromNVS()
{
    esp_err_t ret = ESP_OK;
    bool successFlag = true;
    uint8_t temp = 0;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("wifi"), wifi_restoreVariablesFromNVS_err, TAG, "nvs->openNVSStorage('wifi') failed");

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifi namespace start");

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
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK       is " + std::to_string(runStackSizeK));
        }

        if (ret != ESP_OK)
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore runStackSizeK");
            successFlag = false;
        }
    }

    if (successFlag) // Restore autoConnect
    {
        ret = nvs->readBooleanFromNVS("autoConnect", &autoConnect);

        if (ret == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): autoConnect         is " + std::to_string(autoConnect));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore autoConnect. Error = " + esp_err_to_name(ret));
        }
    }

    if (successFlag) // Restore hostStatus
    {
        if (nvs->readU8IntegerFromNVS("hostStatus", &hostStatus) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): hostStatus          is " + std::to_string(hostStatus));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore hostStatus");
        }
    }

    if (successFlag) // Restore ssidPri
    {
        if (nvs->readStringFromNVS("ssidPri", &ssidPri) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): ssidPri             is " + ssidPri);
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore ssidPri");
        }
    }

    if (successFlag) // Restore ssidPwdPri
    {
        if (nvs->readStringFromNVS("ssidPwdPri", &ssidPwdPri) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): ssidPwdPri          is " + ssidPwdPri);
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore ssidPwdPri");
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifi namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Success");
    }
    else
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

wifi_restoreVariablesFromNVS_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}

void Wifi::saveVariablesToNVS()
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
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("wifi"), wifi_saveVariablesToNVS_err, TAG, "nvs->openNVSStorage('wifi') failed");

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifi namespace start");

    if (successFlag) // Save runStackSizeK
    {
        if (nvs->writeU8IntegerToNVS("runStackSizeK", runStackSizeK) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK       = " + std::to_string(runStackSizeK));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to writeU8IntegerToNVS runStackSizeK");
        }
    }

    if (successFlag) // Save autoConnect
    {
        if (nvs->writeBooleanToNVS("autoConnect", autoConnect) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): autoConnect         = " + std::to_string(autoConnect));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save autoConnect");
        }
    }

    if (successFlag) // Save hostStatus
    {
        if (nvs->writeU8IntegerToNVS("hostStatus", hostStatus) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): hostStatus          = " + std::to_string(hostStatus));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save hostStatus");
        }
    }

    if (successFlag) // Save ssidPri
    {
        if (nvs->writeStringToNVS("ssidPri", &ssidPri) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): ssidPri             = " + ssidPri);
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save ssidPri");
        }
    }

    if (successFlag) // Save ssidPwdPri
    {
        if (nvs->writeStringToNVS("ssidPwdPri", &ssidPwdPri) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): ssidPwdPri          = " + ssidPwdPri);
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save ssidPwdPri");
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifi namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Success");
    }
    else
        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

wifi_saveVariablesToNVS_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}
