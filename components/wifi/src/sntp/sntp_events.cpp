#include "sntp/sntp_.hpp"
#include "system_.hpp" // Class structure and variables

extern SNTP *ptrSNTPInternal;

extern SemaphoreHandle_t semSNTPRouteLock;

/* Event Callback Functions - SNTP */
void SNTP::eventHandlerSNTPMarshaller(struct timeval *tv)
{
    ptrSNTPInternal->eventHandlerSNTP(tv);
}

//
// NOTE:  We do not allow long processes to execute inside this handler.  Our goal is to release the calling task quickly because it has
// other more pressing work to handle.  All events are off-loaded to the standard run process.
//
void SNTP::eventHandlerSNTP(struct timeval *tv)
{
    SNTP_Event evt;
    evt.blnTimeArrived = true;

    if (show & _showEvents)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): Called by the task named: " + std::string(pcTaskGetName(NULL)));

    if (show & _showEvents)
        logByValue(ESP_LOG_INFO, semSNTPRouteLock, TAG, std::string(__func__) + "(): evt.blnTimeArrived is " + std::to_string(evt.blnTimeArrived));

    if (xQueueSendToBack(queueEvents, &evt, 0) == pdFALSE)
        logByValue(ESP_LOG_ERROR, semSNTPRouteLock, TAG, std::string(__func__) + "(): SNTP Event queue over-flowed!");
}