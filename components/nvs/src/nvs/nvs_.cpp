#include "nvs/nvs_.hpp"

#include "esp_efuse.h"
#include "esp_efuse_table.h"

/* Local Semaphores */
SemaphoreHandle_t semNVSEntry = NULL; // Varible lives in this translation unit.
SemaphoreHandle_t semNVSRouteLock = NULL;

//
// Previously, NVS functions were hosted within the System object, but we are increasing NVS services so now those functions are being moved away from the System.
// This reduces the need to lock the System object during NVS activities.
//
// This object has no task running and therefore no FreeRTOS queue or notification mechanisms are used to gain access to it.  Again, like in the System object,
// we use a locking semaphore to gain access to NVS for all our mid-level storage and retrieval needs.
//
// Also like the System, this NVS object is a singleton object and remains instantiated for the lifetime of the application - UNLESS, the system shuts down and
// puts the system to sleep.  In the case of a shut-down, nvs is destroyed.
//
// NVS Error codes can be found in nvs.h
//

/* Construction / Destruction */
NVS::NVS()
{
    setFlags();                // Static enabling of logging statements for any area of concern during development.
    setLogLevels();            // Manually sets log levels for tasks down the call stack for development.
    createSemaphores();        // Creates any locking semaphores owned by this object.
    restoreVariablesFromNVS(); // Brings back all our persistant data.
    initializeNVS();           // We don't have a run task, we all our initialization is done here.
}

void NVS::setFlags()
{
    // show variable is system wide defined and this exposes for viewing any general processes.
    show = 0;
    // show |= _showNVS;
    // show |= _showRun; // We don't use any of these flags below _showNVS
    // show |= _showEvents;
    // show |= _showJSONProcessing;
    // show |= _showDebugging;
    // show |= _showProcess;
    // show |= _showPayload;
}

void NVS::setLogLevels()
{
    if (show > 0)                             // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.
}

void NVS::createSemaphores()
{
    semNVSEntry = xSemaphoreCreateBinary(); // External Entry locking
    if (semNVSEntry != NULL)
        xSemaphoreGive(semNVSEntry);
}

void NVS::restoreVariablesFromNVS()
{
    // NVS doens't maintain any variables at this time.
}

void NVS::initializeNVS()
{
    xSemaphoreTake(semNVSEntry, portMAX_DELAY);

    esp_err_t ret = nvs_flash_init();

    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NOT_FOUND) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): ********** Erasing NVS for use. **********");
        ESP_ERROR_CHECK(nvs_flash_erase()); // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_init());  // Retry nvs_flash_init
    }

    xSemaphoreGive(semNVSEntry);
}

/* Public Member Functions */
void NVS::eraseNVSPartition(const char str[])
{
    ESP_ERROR_CHECK(nvs_flash_erase_partition(str));
    logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): NVS Erased partition " + std::string(str));
}

void NVS::eraseNVSNamespace(char str[])
{
    ESP_ERROR_CHECK(openNVSStorage(str));
    ESP_ERROR_CHECK(nvs_erase_all(nvsHandle));
    ESP_ERROR_CHECK(closeNVStorage());
    logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): NVS Erased namespace " + std::string(str));
}

esp_err_t NVS::openNVSStorage(const char *name_space)
{
    ESP_RETURN_ON_ERROR(nvs_open(name_space, NVS_READWRITE, &nvsHandle), TAG, "nvs_open() failed...");
    return ESP_OK;
}

esp_err_t NVS::closeNVStorage()
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_FAIL;
    }

    // On some reads, we save a default value where no value previously exists -- so all our reads and writes need to be committed.
    ESP_RETURN_ON_ERROR(nvs_commit(nvsHandle), TAG, "nvs_commit() failed...");
    nvs_close(nvsHandle);
    nvsHandle = 0;
    return ESP_OK;
}

esp_err_t NVS::readBooleanFromNVS(const char *key, bool *value)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in a key of: " + std::string(key));

    esp_err_t ret = ESP_OK;
    uint8_t intValue = (int)*value; // Copy value converting boolean to integer.

    ret = nvs_get_u8(nvsHandle, key, &intValue);

    if (ret == ESP_OK)
    {
        if (intValue > 1)
        {
            logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): Improper value of " + std::to_string(intValue) + "stored");
            return ESP_FAIL;
        }

        *value = (bool)intValue; // values of 0 and 1 should convert correctly to bool.
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // If the call to nvs_get_u8 fails, the default value passed in by reference is unchanged.  We use that value for a first time save.
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): New Bool value stored as u8int with key of " + std::string(key) + " = " + std::to_string(intValue));
        return nvs_set_u8(nvsHandle, key, intValue);
    }
    else // Unexpected Error
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): nvs_get_u8(nvsHandle, key, &val) failed, code = " + esp_err_to_name(ret));

    return ret;
}

esp_err_t NVS::writeBooleanToNVS(const char *key, bool newValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + std::to_string(newValue));

    uint8_t storedValue = 0;

    // We try to do a read first.  If entry in NVS doesn't exist, the read will create the entry for us with a starting value of newValue.
    // If the entry already exists, we get back its current value.  If new value is different, we save it.  We don't try to resave
    // a value that is unchanged.
    esp_err_t ret = readU8IntegerFromNVS(key, &storedValue);

    if (ret == ESP_OK)
    {
        if (newValue != storedValue) // If the value has changed or empty, then update with new value
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): writeU8IntegerToNVS is key " + std::string(key) + " with value of: " + std::to_string(newValue));

            ret = nvs_set_u8(nvsHandle, key, (uint8_t)newValue); // Casts the boolean to an integer
        }
    }
    else
    {
        if (show & _showNVS) // Unexpected Error
            logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): writeU8IntegerToNVS failed esp_err_t code = " + esp_err_to_name(ret));
    }

    if (show & _showNVS)
    {
        std::string trueFalse = "";

        if (newValue)
            trueFalse = "true";
        else
            trueFalse = "false";

        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): saveBooleanAsString key " + std::string(key) + " with value of: " + std::to_string(newValue));
    }

    return ret;
}

esp_err_t NVS::readStringFromNVS(const char *key, std::string *strValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in a key of: " + std::string(key));

    esp_err_t ret;
    std::string storedValue = "";
    size_t storedValueLength = 0;

    ret = nvs_get_str(nvsHandle, key, NULL, &storedValueLength); // First ask for the length of the stored string

    if ((storedValueLength > 0) && (storedValueLength < ESP_ERR_NVS_BASE)) // If the size is above zero and less than an error code
    {
        // We create a temporary copy buffer here, but there may be a better way of doing this.
        char *tempChars = (char *)malloc(storedValueLength);
        taskYIELD();

        ret = nvs_get_str(nvsHandle, key, tempChars, &storedValueLength); // Something should be there because we have a length

        storedValue = std::string(tempChars);
        free(tempChars);

        if (ret == ESP_OK)
        {
            *strValue = storedValue;

            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Retrieved " + std::string(key) + " from NVS of: " + std::string(storedValue)); // Debug print statements

            return ret;
        }
    }
    else if ((ret == ESP_ERR_NVS_NOT_FOUND) || (ret < ESP_ERR_NVS_BASE)) // Stored value doesn't exist OR stored value DOES exist is empty (no error)
        return nvs_set_str(nvsHandle, key, strValue->c_str());           // Save the value which was passed to our function by reference.

    logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): readStringFromNVS() failed for some reason, code = " + esp_err_to_name(ret));
    return ret;
}

esp_err_t NVS::writeStringToNVS(const char *key, std::string *newValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + *newValue);

    esp_err_t ret = ESP_OK;
    char *tempChars;
    std::string storedValue = "";
    size_t storedValueLength = 0;

    ret = nvs_get_str(nvsHandle, key, NULL, &storedValueLength);

    if ((storedValueLength > 0) && (storedValueLength < ESP_ERR_NVS_BASE)) // We have a previously stored value.
    {
        tempChars = (char *)malloc(storedValueLength);
        taskYIELD();

        ret = nvs_get_str(nvsHandle, key, tempChars, &storedValueLength);
        taskYIELD();

        storedValue = std::string(tempChars); // Transfer value over to string.  This simplifies our code in subsequent steps.
        free(tempChars);                      // Don't allow a memory leak!

        if (storedValue == *newValue) // storedValue is equal to newValue. Do not access nvs.  We are done
            return ESP_OK;
        else
            return nvs_set_str(nvsHandle, key, newValue->c_str()); // storedValue and newValue are different so we save the newValue.  We are done.
    }
    else if (ret == ESP_ERR_NVS_NOT_FOUND)
        return nvs_set_str(nvsHandle, key, newValue->c_str()); // Save the new value which was passed to our function by reference.  We are done.

    logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): writeStringToNVS() failed for some reason, code = " + esp_err_to_name(ret));
    return ret;
}

esp_err_t NVS::readU8IntegerFromNVS(const char *key, uint8_t *intValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in a key of: " + std::string(key));

    esp_err_t ret = nvs_get_u8(nvsHandle, key, intValue);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // If the call to nvs_get_u32 fails, the default value passed in by reference is unchanged.  We use that value to save for the first time.
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): New value stored as u8int with key of " + std::string(key) + " = " + std::to_string(*intValue));
        return nvs_set_u8(nvsHandle, key, *intValue);
    }
    else if (ret != ESP_OK) // Unexpected Error
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): read failed esp_err_t code = " + esp_err_to_name(ret));
    return ret;
}

esp_err_t NVS::writeU8IntegerToNVS(const char *key, uint8_t newValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + std::to_string(newValue));

    uint8_t storedValue = 0;

    // We try to do a read first.  If entry in NVS doesn't exist, the read will create the entry for us with a starting value of newValue.
    // If the entry already exists, we get back its current value.  If new value is different, we save it.  We don't try to resave
    // a value that is unchanged.
    esp_err_t ret = readU8IntegerFromNVS(key, &storedValue);

    if (ret == ESP_OK)
    {
        if (newValue != storedValue) // If the value has changed or empty, then update with new value
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): writeU8IntegerToNVS is key " + std::string(key) + " with value of: " + std::to_string(newValue));

            ret = nvs_set_u8(nvsHandle, key, newValue);
        }
    }
    else
    {
        if (show & _showNVS) // Unexpected Error
            logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): writeU8IntegerToNVS failed esp_err_t code = " + esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t NVS::readI32IntegerFromNVS(const char *key, int32_t *intValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in a key of: " + std::string(key));

    esp_err_t ret = nvs_get_i32(nvsHandle, key, intValue);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // If the call to nvs_get_i32 fails, the default value passed in by reference is unchanged.  We use that value to save for the first time.
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): New value stored as i32int with key of " + std::string(key));
        return nvs_set_i32(nvsHandle, key, *intValue);
    }
    else if (ret != ESP_OK) // Unexpected Error
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): read failed esp_err_t code = " + esp_err_to_name(ret));
    return ret;
}

esp_err_t NVS::writeI32IntegerToNVS(const char *key, int32_t newValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + std::to_string(newValue));

    int32_t storedValue = 0;

    // We try to do a read first.  If entry in NVS doesn't exist, the read will create the entry for us with a starting value of 0.
    // If the entry already exists, we get back its current value.  If new value is different, we save it.  We don't try to resave a value that is unchanged
    esp_err_t ret = readI32IntegerFromNVS(key, &storedValue);

    if (ret == ESP_OK)
    {
        if (newValue != storedValue) // If the value has changed or empty, then update with new value
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): writeI32IntegerToNVS is key " + std::string(key) + " with value of: " + std::to_string(newValue));

            ret = nvs_set_i32(nvsHandle, key, newValue);
        }
    }

    if (ret != ESP_OK)
    {
        if (show & _showNVS) // Unexpected Error
            logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): writeI32IntegerToNVS failed esp_err_t code = " + esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t NVS::readU32IntegerFromNVS(const char *key, uint32_t *intValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
       logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in a key of: " + std::string(key));

    esp_err_t ret = nvs_get_u32(nvsHandle, key, intValue);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // If the call to nvs_get_u32 fails, the default value passed in by reference is unchanged.  We use that value to save for the first time.
        logByValue(ESP_LOG_WARN, semNVSRouteLock, TAG, std::string(__func__) + "(): New value stored as u32int with key of " + std::string(key));
        return nvs_set_u32(nvsHandle, key, *intValue);
    }
    else if (ret != ESP_OK) // Unexpected Error
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): Read failed esp_err_t code = " + esp_err_to_name(ret));
    return ret;
}

esp_err_t NVS::writeU32IntegerToNVS(const char *key, uint32_t newValue)
{
    if (nvsHandle == 0)
    {
        logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): You must openNVSStorage() first!");
        return ESP_ERR_NVS_INVALID_HANDLE;
    }

    if (show & _showNVS)
        logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + std::to_string(newValue));

    uint32_t storedValue = 0;

    // We try to do a read first.  If entry in NVS doesn't exist, the read will create the entry for us with a starting value of 0.
    // If the entry already exists, we get back its current value.  If new value is different, we save it.  We don't try to resave a value that is unchanged
    esp_err_t ret = readU32IntegerFromNVS(key, &storedValue);

    if (ret == ESP_OK)
    {
        if (newValue != storedValue) // If the value has changed, then update with new value
        {
            if (show & _showNVS)
                logByValue(ESP_LOG_INFO, semNVSRouteLock, TAG, std::string(__func__) + "(): Passed in key " + std::string(key) + " with value of: " + std::to_string(newValue));

            ret = nvs_set_u32(nvsHandle, key, newValue);
        }
    }
    else
    {
        if (show & _showNVS) // Unexpected Error
            logByValue(ESP_LOG_ERROR, semNVSRouteLock, TAG, std::string(__func__) + "(): writeU32IntegerToNVS failed esp_err_t code = " + esp_err_to_name(ret));
    }
    return ret;
}