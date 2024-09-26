#include "wifi/wifi_.hpp"

/* Local Semaphores */
extern SemaphoreHandle_t semWifiRouteLock;

/* Event Callback Functions - Wifi */
void Wifi::eventHandlerWifiMarshaller(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ((Wifi *)arg)->eventHandlerWifi(event_base, event_id, event_data);
}

void Wifi::eventHandlerWifi(esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WIFI_Event evt; // NOTE: These events do not send any data, so we are not trying to package any data here.
    evt.event_base = event_base;
    evt.event_id = event_id;

    if (show & _showEvents)
        logByValue(ESP_LOG_INFO, semWifiRouteLock, TAG, std::string(__func__) + "(): Called by the task named: " + std::string(pcTaskGetName(NULL)));

    if (show & _showEvents)
        logByValue(ESP_LOG_INFO, semWifiRouteLock, TAG, std::string(__func__) + "(): evt.event_base is " + std::string(evt.event_base) + " evt.event_id is " + std::to_string(evt.event_id));

    if (xQueueSendToBack(queueEvents, &evt, 0) == pdFALSE)
        logByValue(ESP_LOG_INFO, semWifiRouteLock, TAG, std::string(__func__) + "(): Wifi Event queue over-flowed!");
}