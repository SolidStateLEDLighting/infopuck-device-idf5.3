#include "display/display_.hpp"
#include "system_.hpp"

/* External Semaphores */
extern SemaphoreHandle_t semDisplayEntry;

void Display::runMarshaller(void *arg)
{
    ((Display *)arg)->run();
    vTaskDelete(NULL);
    ((Display *)arg)->taskHandleRun = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
}

void Display::run(void)
{
    // esp_err_t ret = ESP_OK;
    uint8_t cadenceTimeDelay = 250;

    DISPLAY_NOTIFY dispTaskNotifyValue = static_cast<DISPLAY_NOTIFY>(0);

    while (true)
    {
        //
        // In every pass, we examine Task Notifications and/or the Command Request Queue.  The extra bonus we get here is that this is our yield
        // time back to the scheduler.  We don't need to perform another yield anywhere else to cooperatively yield to the OS.
        // If we have things to do, we maintain cadenceTimeDelay = 0, but if all processes are finished, then we will place 250mSec delay time in
        // cadenceTimeDelay for a Task Notification wait.  This permits us to reduce power consumption when we are not busy without sacrificing latentcy
        // when we are busy.  Relaxed schduling with 250mSec equates to about a 4Hz run() loop cadence.
        //
        dispTaskNotifyValue = static_cast<DISPLAY_NOTIFY>(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cadenceTimeDelay)));

        if (dispTaskNotifyValue > static_cast<DISPLAY_NOTIFY>(0)) // Looking for Task Notifications
        {
            // Task Notifications should be used for notifications (NFY_NOTIFICATION) or commands (CMD_COMMAND) both of which need no input and return no data.
            switch (dispTaskNotifyValue)
            {
            case DISPLAY_NOTIFY::NFY_EMPTY: // Some of these notifications set Directive bits - a follow up CMD_RUN_DIRECTIVES task notification starts the action.
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received DISPLAY_NOTIFY::NFY_EMPTY");
                break;
            }

            case DISPLAY_NOTIFY::CMD_EMPTY:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received DISPLAY_NOTIFY::CMD_EMPTY");
                break;
            }

            case DISPLAY_NOTIFY::CMD_LOG_TASK_INFO:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received DISPLAY_NOTIFY::CMD_LOG_TASK_INFO");
                break;
            }

            case DISPLAY_NOTIFY::CMD_SHUT_DOWN:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received DISPLAY_NOTIFY::CMD_SHUT_DOWN");
                break;
            }
            }
        }
        else // If we don't have a Notification, then look for any Command Requests (thereby handling only one upon each run() loop entry)
        {
            // Queue based commands should be used for commands which provide input and optioanlly return data.   Use a notification if NO data is passed either way.
            if (xQueuePeek(queueCmdRequests, (void *)&ptrDisplayCmdRequest, 0)) // Do I have a command request in the queue?
            {
                if (ptrDisplayCmdRequest != nullptr)
                {
                    if (show & _showPayload)
                    {
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cmd is  " + std::to_string((int)ptrDisplayCmdRequest->requestedCmd));
                    }
                }

                switch (ptrDisplayCmdRequest->requestedCmd)
                {
                case DISPLAY_COMMAND::DO_SOMETHING:
                {
                    if (show & _showRun)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received the command DISPLAY_COMMAND::DO_SOMETHING");
                    saveVariablesToNVS(); // Can't afford a delay here.
                    break;
                }
                }
                xQueueReceive(queueCmdRequests, (void *)&ptrDisplayCmdRequest, pdMS_TO_TICKS(0)); // Remove the item from the queue
            }
        }

        switch (dispOP)
        {
        case DISPLAY_OP::Run:
        {
            break;
        }

        case DISPLAY_OP::Shutdown:
        {
            // Positionally, it is important for Shutdown to be serviced right after it is called for.  We don't want other possible operations
            // becoming active unexepectedly.  This is somewhat important.
            //
            // A shutdown is a complete undoing of all items that were established or created with our run thread.
            // If we created resources, then dispose of them here.
            // NVS actions are canceled.  Shutdown is a rude way to exit immediately.
            switch (dispShdnStep)
            {
            case DISPLAY_SHUTDOWN::Start:
            {
                if (showDisplay & _showDisplayShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): DISPLAY_SHUTDOWN::Start");

                cadenceTimeDelay = 10; // Always allow a bit of delay in Run processing.
                dispShdnStep = DISPLAY_SHUTDOWN::Finished;
                [[fallthrough]];
            }

            case DISPLAY_SHUTDOWN::Finished:
            {
                if (showDisplay & _showDisplayShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Finished");

                // This exits the run function. (notice how the compiler doesn't complain about a missing break statement)
                // In the runMarshaller, the task is deleted and the task handler set to nullptr.
                return;
            }

            case DISPLAY_SHUTDOWN::Error:
            {
                if (showDisplay & _showDisplayShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Error");

                break;
            }
            }
            break;
        }

        case DISPLAY_OP::Init:
        {
            switch (dispInitStep)
            {
            case DISPLAY_INIT::Start:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): DISPLAY_INIT::Start");

                cadenceTimeDelay = 10; // Don't permit scheduler delays in Run processing.

                dispInitStep = DISPLAY_INIT::Finished;
                [[fallthrough]];
            }

            case DISPLAY_INIT::Finished:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): DISPLAY_INIT::Finished");

                cadenceTimeDelay = 250;          // Return to relaxed scheduling.
                xSemaphoreGive(semDisplayEntry); // Allow entry from any other calling tasks
                dispOP = DISPLAY_OP::Run;
                break;
            }

            case DISPLAY_INIT::Error:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): DISPLAY_INIT::Error");
                break;
            }
            }
            break;
        }

        case DISPLAY_OP::Error:
        {
            cadenceTimeDelay = 250; // Return to relaxed scheduling.

            routeLogByValue(LOG_TYPE::ERROR, errMsg);
            dispOP = DISPLAY_OP::Idle;
            [[fallthrough]];
        }

        case DISPLAY_OP::Idle:
        {
            cadenceTimeDelay = 250; // Return to relaxed scheduling.
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        }
    }
}