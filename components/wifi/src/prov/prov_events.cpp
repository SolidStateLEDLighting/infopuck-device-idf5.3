#include "prov/prov_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

/* Event Callback Functions - Provision */
void PROV::eventHandlerProvisionMarshaller(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ((PROV *)arg)->eventHandlerProvision(event_base, event_id, event_data);
}

void PROV::eventHandlerProvision(esp_event_base_t event_base, int32_t event_id, void *event_data) // The System Event task (sys_evt) will arrive here to drive this handler.
{
    // routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Called by the task named: " + std::string(pcTaskGetName(NULL)));
    //
    // The idea of our event handlers is to copy the event data that arrives over to a queue (queueEvents).  The run task will pull that event
    // from queueEvents inside the Run task so all the variables are clear to be operated on without any confict between tasks.
    //
    PROV_Event evt;
    evt.event_base = event_base;
    evt.event_id = event_id;
    //
    // Received data can be of different sizes depending on the event.
    //
    memset(&evt.data, 0, sizeof(evt.data)); // Clear the entire length of the data buffer.

    if ((event_base == WIFI_PROV_EVENT) && (event_id == WIFI_PROV_CRED_RECV))
        memcpy(&evt.data, event_data, sizeof(wifi_sta_config_t));
    else if ((event_base == WIFI_PROV_EVENT) && (event_id == WIFI_PROV_CRED_FAIL))
        memcpy(&evt.data, event_data, sizeof(wifi_prov_sta_fail_reason_t));

    if (show & _showEvents)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): evt.event_base is " + std::string(evt.event_base) + " evt.event_id is " + std::to_string(evt.event_id));

    if (xQueueSendToBack(queueEvents, &evt, 0) == pdFALSE)
        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Provision Event queue over-flowed!");

    // ESP_LOGW(TAG, "queueEvents size is %d", uxQueueMessagesWaiting(queueEvents));
}

esp_err_t PROV::eventHandlerCustomMarshaller(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *arg)
{
    return ((PROV *)arg)->eventHandlerCustom(session_id, inbuf, inlen, outbuf, outlen);
}

esp_err_t PROV::eventHandlerCustom(uint32_t session_id, const uint8_t *inputBuffer, ssize_t inputBufferLength, uint8_t **outputBuffer, ssize_t *outputBufferLength)
{
    ESP_LOGW(TAG, "Task's name is %s", pcTaskGetName(xTaskGetCurrentTaskHandle())); // You may monitor and verfiy the task's name.
    ESP_LOGW(TAG, "eventHandlerCustom _________________________________________");  // This area is not working yet.

    if (inputBuffer)
        ESP_LOGW(TAG, "%s", inputBuffer); // This area is not working yet.

    char response[] = "SUCCESS";

    *outputBuffer = (uint8_t *)strdup(response);

    ESP_RETURN_ON_FALSE(*outputBuffer, ESP_ERR_NO_MEM, TAG, "System out of memory");

    *outputBufferLength = strlen(response) + 1; // +1 for NULL terminating byte

    return ESP_OK;
}
