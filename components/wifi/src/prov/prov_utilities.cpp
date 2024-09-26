#include "prov/prov_.hpp"

/* Local Semaphores */
extern SemaphoreHandle_t semProvRouteLock;

void PROV::getDeviceServiceName(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/* Provisioning Security Support Routines */
esp_err_t PROV::example_get_sec2_salt(const char **salt, uint16_t *salt_len)
{
    if (blnSec2DevelopmentMode)
    {
        logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): Development mode: using hard coded salt");

        *salt = sec2_salt;
        *salt_len = sizeof(sec2_salt);

        esp_log_buffer_hexdump_internal(TAG, *salt, *salt_len, ESP_LOG_WARN);
        logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): salt length is " + std::to_string(*salt_len));
    }
    else if (blnSec2ProductionMode)
    {
        // errMsg = std::string(__func__) + "(): blnSec2ProductionMode Not implemented!";
        logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, std::string(__func__) + "(): blnSec2ProductionMode Not implemented!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t PROV::example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
    if (blnSec2DevelopmentMode)
    {
        logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): Development mode: using hard coded verifier");

        *verifier = sec2_verifier;
        *verifier_len = sizeof(sec2_verifier);

        esp_log_buffer_hexdump_internal(TAG, *verifier, *verifier_len, ESP_LOG_WARN);
        logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): verifier length is " + std::to_string(*verifier_len));
    }
    else if (blnSec2ProductionMode)
    {
        errMsg = std::string(__func__) + "(): blnSec2ProductionMode Not implemented!"; // This code needs to be updated with appropriate implementation to provide verifier
        logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, errMsg);
        return ESP_FAIL;
    }
    return ESP_OK;
}
