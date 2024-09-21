/** YOU MUST VIEW THIS PROJECT IN VS CODE TO SEE FOLDING AND THE PERFECTION OF THE DESIGN **/

#include "wifi/wifi_.hpp"
#include "system_.hpp"

#include "esp_check.h"

/* External Semaphores */
extern SemaphoreHandle_t semWifiEntry;
extern SemaphoreHandle_t semProvEntry;

void Wifi::runMarshaller(void *arg)
{
    ((Wifi *)arg)->run();
    vTaskDelete(NULL);
    ((Wifi *)arg)->taskHandleWIFIRun = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
}

void Wifi::run(void)
{
    esp_err_t ret = ESP_OK;

    uint8_t cadenceTimeDelay = 250;
    bool cmdRunDirectives = false;

    uint32_t wifiConnStartTicks = 0;     // Starting point for..
    uint8_t waitingOnHostConnSec = 0;    //
    uint8_t waitingOnIPAddressSec = 0;   // Single second counters
    uint8_t noValidTimeSecToRestart = 0; //

    WIFI_NOTIFY wifiTaskNotifyValue = static_cast<WIFI_NOTIFY>(0);

    while (true)
    {
        if (uxQueueMessagesWaiting(queueEvents)) // We always give top priorty to handling events
            runEvents();
        //
        // In every pass, we examine Task Notifications and/or the Command Request Queue.  The extra bonus we get here is that this is our yield
        // time back to the scheduler.  We don't need to perform another yield anywhere else to cooperatively yield to the OS.
        // If we have things to do, we maintain cadenceTimeDelay = 0, but if all processes are finished, then we will place 250mSec delay time in
        // cadenceTimeDelay for a Task Notification wait.  This permits us to reduce power consumption when we are not busy without sacrificing latentcy
        // when we are busy.  Relaxed schduling with 250mSec equates to about a 4Hz run() loop cadence.
        //
        wifiTaskNotifyValue = static_cast<WIFI_NOTIFY>(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cadenceTimeDelay)));

        if (wifiTaskNotifyValue > static_cast<WIFI_NOTIFY>(0)) // Looking for Task Notifications
        {
            // Task Notifications should be used for notifications (NFY_NOTIFICATION) or commands (CMD_COMMAND) both of which need no input and return no data.
            switch (wifiTaskNotifyValue)
            {
            case WIFI_NOTIFY::CMD_CLEAR_PRI_HOST: // Some of these notifications set Directive bits - a follow up CMD_RUN_DIRECTIVES task notification starts the action.
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_CLEAR_PRI_HOST");
                wifiDirectives |= _wifiClearPriHostInfo;
                break;
            }

            case WIFI_NOTIFY::CMD_DISC_HOST:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_DISC_HOST");
                wifiDirectives |= _wifiDisconnectHost;
                break;
            }

            case WIFI_NOTIFY::CMD_CONN_PRI_HOST:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_CONN_PRI_HOST");
                wifiDirectives |= _wifiConnectPriHost;
                break;
            }

            case WIFI_NOTIFY::CMD_PROV_HOST:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_CONN_PRI_HOST");
                wifiDirectives |= _wifiProvisionPriHost;
                break;
            }

            case WIFI_NOTIFY::CMD_RUN_DIRECTIVES:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_RUN_DIRECTIVES");
                cmdRunDirectives = true;
                break;
            }

            case WIFI_NOTIFY::CMD_SET_AUTOCONNECT:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_SET_AUTOCONNECT");

                autoConnect = true;
                saveVariablesToNVS();
                break;
            }

            case WIFI_NOTIFY::CMD_CLEAR_AUTOCONNECT:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_CLEAR_AUTOCONNECT");

                autoConnect = false;
                saveVariablesToNVS();
                break;
            }

            case WIFI_NOTIFY::CMD_SHUT_DOWN:
            {
                if (show & _showRun)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received WIFI_NOTIFY::CMD_SHUT_DOWN");

                wifiShdnStep = WIFI_SHUTDOWN::Start;
                wifiOP = WIFI_OP::Shutdown;
                break;
            }
            }
        }
        else // If we don't have a Notification, then look for any Command Requests (thereby handling only one upon each run() loop entry)
        {
            // Queue based commands should be used for commands which provide input and optioanlly return data.   Use a notification if NO data is passed either way.
            if (xQueuePeek(queueCmdRequests, (void *)&ptrWifiCmdRequest, 0)) // Do I have a command request in the queue?
            {
                if (ptrWifiCmdRequest != nullptr)
                {
                    if (show & _showPayload)
                    {
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cmd is  " + std::to_string((int)ptrWifiCmdRequest->requestedCmd));
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): data is " + std::string((char *)(ptrWifiCmdRequest->data1)));
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): len  is " + std::to_string(ptrWifiCmdRequest->data1Length));
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): data is " + std::string((char *)(ptrWifiCmdRequest->data2)));
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): len  is " + std::to_string(ptrWifiCmdRequest->data2Length));
                    }
                }

                switch (ptrWifiCmdRequest->requestedCmd)
                {
                case WIFI_COMMAND::SET_SSID_PRI:
                {
                    if (show & _showRun)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received the command WIFI_COMMAND::SET_SSID_PRI");

                    ssidPri = std::string((char *)ptrWifiCmdRequest->data1, (size_t)ptrWifiCmdRequest->data1Length);
                    ssidPwdPri = std::string((char *)ptrWifiCmdRequest->data2, (size_t)ptrWifiCmdRequest->data2Length);
                    hostStatus |= _hostPriValid; // If we don't declare the SSID/Password as valid, the algorithm won't allow a connection.
                    saveVariablesToNVS();        // Can't afford a delay here.
                    break;
                }

                case WIFI_COMMAND::SET_WIFI_CONN_STATE:
                {
                    wifiConnState = (WIFI_CONN_STATE)ptrWifiCmdRequest->data1[0];
                    break;
                }

                case WIFI_COMMAND::SET_SHOW_FLAGS:
                {
                    if (show & _showRun)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Received the command WIFI_COMMAND::SET_SHOW_FLAGS");

                    show = ptrWifiCmdRequest->data1[0];
                    showWifi = ptrWifiCmdRequest->data1[1];
                    setLogLevels();
                    break;
                }
                }
                xQueueReceive(queueCmdRequests, (void *)&ptrWifiCmdRequest, pdMS_TO_TICKS(0)); // Remove the item from the queue
            }
        }

        switch (wifiOP)
        {
        case WIFI_OP::Run:
        {
            if (cmdRunDirectives) // Do we have any directives?  Run them when all other commands are Finished.
            {
                if ((wifiShdnStep == WIFI_SHUTDOWN::Finished) &&
                    (wifiInitStep == WIFI_INIT::Finished) &&
                    (wifiConnStep == WIFI_CONN::Finished) &&
                    (wifiDiscStep == WIFI_DISC::Finished))
                {
                    cmdRunDirectives = false;                    // Clear the flag
                    wifiDirectivesStep = WIFI_DIRECTIVES::Start; //
                    wifiOP = WIFI_OP::Directives;                //
                    break;                                       // Exit out and allow Directives to run
                }
            }

            sntp->run(); // We still need to enter SNTP can it process server events.
            break;
        }

        case WIFI_OP::Shutdown:
        {
            // Positionally, it is important for Shutdown to be serviced right after it is called for.  We don't want other possible operations
            // becoming active unexepectedly.  This is somewhat important.
            //
            // A shutdown is a complete undoing of all items that were established or created with our run thread.
            // If we have Directives, cancel them.  If we are connected then disconnect.  If we created resources, then dispose of them here.
            // NVS actions are also canceled.  Shutdown is a rude way to exit immediately.
            switch (wifiShdnStep)
            {
            case WIFI_SHUTDOWN::Start:
            {
                if (showWifi & _showWifiShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Start");

                cadenceTimeDelay = 10; // Always allow a bit of delay in Run processing.
                wifiShdnStep = WIFI_SHUTDOWN::Cancel_Directives;
                [[fallthrough]];
            }

            case WIFI_SHUTDOWN::Cancel_Directives:
            {
                if (showWifi & _showWifiShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Cancel_Directives - Step " + std::to_string((int)WIFI_SHUTDOWN::Cancel_Directives));

                wifiDirectivesStep = WIFI_DIRECTIVES::Finished; // Immediately cancel out all Directives...
                wifiDirectives = 0;
                cmdRunDirectives = false;

                wifiShdnStep = WIFI_SHUTDOWN::Disconnect_Wifi;
                break;
            }

            case WIFI_SHUTDOWN::Disconnect_Wifi:
            {
                if (showWifi & _showWifiShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Disconnect_Wifi - Step " + std::to_string((int)WIFI_SHUTDOWN::Disconnect_Wifi));

                wifiDiscStep = WIFI_DISC::Start;
                wifiOP = WIFI_OP::Disconnect;

                wifiShdnStep = WIFI_SHUTDOWN::Wait_For_Disconnection; // Resume upon return at this step.
                break;
            }

            case WIFI_SHUTDOWN::Wait_For_Disconnection:
            {
                if (showWifi & _showWifiShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Wait_For_Disconnection - Step " + std::to_string((int)WIFI_SHUTDOWN::Wait_For_Disconnection));

                if (wifiConnState != WIFI_CONN_STATE::WIFI_DISCONNECTED) //
                    break;                                               // Not disconnected yet

                wifiShdnStep = WIFI_SHUTDOWN::Finished; // All finished
                [[fallthrough]];                        //
            }

            case WIFI_SHUTDOWN::Finished:
            {
                if (showWifi & _showWifiShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_SHUTDOWN::Finished");
                // This exits the run function. (notice how the compiler doesn't complain about a missing break statement)
                // In the runMarshaller, the task is deleted and the task handler set to nullptr.
                return;
            }
            }
            break;
        }

        case WIFI_OP::Init:
        {
            switch (wifiInitStep)
            {
            case WIFI_INIT::Start:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Start");

                cadenceTimeDelay = 10;                              // Don't permit scheduler delays in Run processing.
                wifiConnState = WIFI_CONN_STATE::WIFI_DISCONNECTED; // Reset the connection state.

                wifiInitStep = WIFI_INIT::Checks;
                [[fallthrough]];
            }

            case WIFI_INIT::Checks:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Checks - Step " + std::to_string((int)WIFI_INIT::Checks));

                // NOTE: Checking for all the required variables here helps us reduce complexity in the remainder of the code.

                if (sntp == nullptr)
                {
                    errMsg = std::string(__func__) + "(): Who forgot to instantiate sntp?";
                    wifiOP = WIFI_OP::Error; // Effectively, this is an assert without an abort.
                    break;
                }

                if (taskHandleSystemRun == nullptr)
                {
                    errMsg = std::string(__func__) + "(): taskHandleSystemRun is nullptr, but is needed for notifications...";
                    wifiOP = WIFI_OP::Error; // Effectively, this is an assert without an abort.
                    break;
                }

                wifiInitStep = WIFI_INIT::Set_Variables_From_Config;
                [[fallthrough]];
            }

            case WIFI_INIT::Set_Variables_From_Config:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Set_Variables_From_Config - Step " + std::to_string((int)WIFI_INIT::Set_Variables_From_Config));
                //
                // Any value held in Config will over-write any value stored in the system.  Make sure Config values are clear before sending product to the field.
                //
                std::string configValue = CONFIG_ESP_WIFI_STA_SSID_PRI;

                // if the stored nvs value doesn't match the staConfig value and the staConfig value is not "empty", then change to staConfig value.
                if ((ssidPri.compare(configValue) != 0) && (configValue.compare("empty") != 0))
                {
                    ssidPri = CONFIG_ESP_WIFI_STA_SSID_PRI;
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): Using ssidPri    from Configuration with a value of " + ssidPri);
                    saveVariablesToNVS();
                }
                else
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Using ssidPri    from nvs with a value of " + ssidPri);

                configValue = CONFIG_ESP_WIFI_STA_PASSWORD_PRI;

                if ((ssidPwdPri.compare(configValue) != 0) && (configValue.compare("empty") != 0))
                {
                    ssidPwdPri = CONFIG_ESP_WIFI_STA_PASSWORD_PRI;
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): Using ssidPwdPri from Configuration with a value of " + ssidPwdPri);
                    saveVariablesToNVS();
                }
                else
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Using ssidPwdPri from nvs with a value of " + ssidPwdPri);

                wifiInitStep = WIFI_INIT::Auto_Connect;
                [[fallthrough]];
            }

            case WIFI_INIT::Auto_Connect:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Auto_Connect - Step " + std::to_string((int)WIFI_INIT::Auto_Connect));

                if (autoConnect) // If autoConnect is set, then set a directive bit to connect to the active host.
                {
                    wifiDirectives |= _wifiConnectPriHost;
                    saveVariablesToNVS();
                }

                wifiInitStep = WIFI_INIT::Finished;
                break;
            }

            case WIFI_INIT::Finished:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Finished");

                cadenceTimeDelay = 250;       // Return to relaxed scheduling.
                xSemaphoreGive(semWifiEntry); // Allow entry from any other calling tasks
                wifiOP = WIFI_OP::Run;
                break;
            }

            case WIFI_INIT::Error:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_INIT::Error");

                wifiOP = WIFI_OP::Error;
                wifiInitStep = WIFI_INIT::Finished;
                break;
            }
            }
            break;
        }

        case WIFI_OP::Directives:
        {
            switch (wifiDirectivesStep)
            {
            case WIFI_DIRECTIVES::Start:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Start");
                //
                // Directives are stored and executed in a logical order.
                // All directives are cleared once executed.
                //
                // We can manaually test all our directives here on startup.
                // wifiDirectives = 0;
                // wifiDirectives |= _wifiClearPriHostInfo;
                // wifiDirectives |= _wifiDisconnectHost;
                // wifiDirectives |= _wifiConnectPriHost;
                //
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifiDirectives = " + std::to_string(wifiDirectives));

                if (wifiDirectives > 0)
                    wifiDirectivesStep = WIFI_DIRECTIVES::Clear_Data;
                else
                    wifiDirectivesStep = WIFI_DIRECTIVES::Finished;

                [[fallthrough]]; // We really don't need an RTOS yield here.
            }

            case WIFI_DIRECTIVES::Clear_Data:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Clear_Data - Step " + std::to_string((int)WIFI_DIRECTIVES::Clear_Data));

                //
                // Clear any data as required.  This is a one type action that is cleared from the Directives.
                //
                if (wifiDirectives & _wifiClearPriHostInfo)
                {
                    wifiDirectives &= ~_wifiClearPriHostInfo; // Clear this flag
                    ssidPri = "empty";
                    ssidPwdPri = "empty";
                    hostStatus &= ~_hostPriValid; // Can't be valid or active when cleared.
                    hostStatus &= ~_hostPriActive;
                    saveVariablesToNVS(); // Can't afford a delay here.
                }

                wifiDirectivesStep = WIFI_DIRECTIVES::Disconnect_Host;
                break;
            }

            case WIFI_DIRECTIVES::Disconnect_Host:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Disconnect_Host - Step " + std::to_string((int)WIFI_DIRECTIVES::Disconnect_Host));
                //
                // Disconnect any host that is active. This a one action that is cleared from the Directives.
                // We might call to Disconnect a Host than is in service, but this is rare and typically be used when
                // the device is in a Wifi mesh and does not need to be connected to a Host.
                //
                if (wifiDirectives & _wifiDisconnectHost)
                {
                    wifiDirectives &= ~_wifiDisconnectHost; // Cancel this flag.

                    if (autoConnect) // If true, set to false because we are explicitly disconnecting and we don't want the system to reconnect.
                    {
                        autoConnect = false;
                        saveVariablesToNVS();
                    }

                    wifiDiscStep = WIFI_DISC::Start; // Start the disconnection process
                    wifiOP = WIFI_OP::Disconnect;
                    break; // Vector off to Disconnect
                }
                wifiDirectivesStep = WIFI_DIRECTIVES::Connect_Host; // Advanced to the next possible directive
                [[fallthrough]];
            }

            case WIFI_DIRECTIVES::Provision_Host:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Provision_Host - Step " + std::to_string((int)WIFI_DIRECTIVES::Provision_Host));

                if (wifiDirectives & _wifiProvisionPriHost)
                {
                    if (wifiConnState == WIFI_CONN_STATE::WIFI_CONNECTED_STA) // Provisioning must take control of Wifi, so we will disconnect here first.
                    {
                        wifiDiscStep = WIFI_DISC::Start;
                        wifiOP = WIFI_OP::Disconnect;
                        break;
                    }

                    wifiProvStep = WIFI_PROV::Start;
                    wifiOP = WIFI_OP::Provision;

                    wifiDirectives &= ~_wifiProvisionPriHost; // Cancel this flag.
                }

                break;
            }

            case WIFI_DIRECTIVES::Connect_Host:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Connect_Host - Step " + std::to_string((int)WIFI_DIRECTIVES::Connect_Host));

                if (wifiDirectives & _wifiConnectPriHost)
                {
                    wifiDirectives &= ~_wifiConnectPriHost; // Cancel this flag.

                    if (!autoConnect) // If false -- set autoConnect to true
                    {
                        autoConnect = true; // Always set autoConnect UNLESS you manually disconnect...
                        saveVariablesToNVS();
                    }

                    if (wifiConnState != WIFI_CONN_STATE::WIFI_CONNECTED_STA) // Only consider connecting if not connected...
                    {
                        if (showWifi & _showWifiDirectiveSteps)
                            routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): connectPriHost");

                        if (showWifi & _showWifiDirectiveSteps)
                            routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): primary Host Valid");

                        hostStatus |= _hostPriActive; // Indicate that Primary is active
                        saveVariablesToNVS();         // Can't afford a delay here.

                        wifiConnStep = WIFI_CONN::Start;
                        wifiOP = WIFI_OP::Connect;
                        break;
                    }
                }

                wifiDirectivesStep = WIFI_DIRECTIVES::Finished;
                [[fallthrough]];
            }

            case WIFI_DIRECTIVES::Finished:
            {
                if (showWifi & _showWifiDirectiveSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DIRECTIVES::Finished");

                if (wifiDirectives > 0) // If another directive has come in, restart our directives process
                    wifiDirectivesStep = WIFI_DIRECTIVES::Start;
                else
                    wifiOP = WIFI_OP::Run;
                break;
            }
            }
            break;
        }

        case WIFI_OP::Connect:
        {
            switch (wifiConnStep)
            {
            case WIFI_CONN::Start:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Start");

                cadenceTimeDelay = 10; // Don't permit scheduler delays in Run processing.
                wifiConnStep = WIFI_CONN::Create_Netif_Objects;
                [[fallthrough]];
            }

            case WIFI_CONN::Create_Netif_Objects:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Create_Netif_Objects - Step " + std::to_string((int)WIFI_CONN::Create_Netif_Objects));

                if (defaultSTANetif == nullptr)
                {
                    defaultSTANetif = esp_netif_create_default_wifi_sta();
                    ESP_GOTO_ON_FALSE(defaultSTANetif, ESP_FAIL, wifi_Create_Netif_STA_Object_err, TAG, "esp_netif_create_default_wifi_sta() returned nullptr.");
                }
                else
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): WIFI_CONN::Create_Netif_Objects: defaultSTANetif NOT NULL at the start of the Connection, what happened?");

                wifiConnStep = WIFI_CONN::Wifi_Init;
                break;

            wifi_Create_Netif_STA_Object_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Create_Netif_Objects: error " + esp_err_to_name(ret);
                wifiConnStep = WIFI_CONN::Error;
                break;
            }

            case WIFI_CONN::Wifi_Init:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_Init - Step " + std::to_string((int)WIFI_CONN::Wifi_Init));

                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                ESP_GOTO_ON_ERROR(esp_wifi_init(&cfg), wifi_Wifi_Init_err, TAG, "esp_wifi_init(&cfg) failed");
                wifiConnStep = WIFI_CONN::Register_Handlers;
                break;

            wifi_Wifi_Init_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Wifi_Init: error: " + esp_err_to_name(ret);
                wifiOP = WIFI_OP::Error;
                break;
            }

            case WIFI_CONN::Register_Handlers:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Register_Handlers - Step " + std::to_string((int)WIFI_CONN::Register_Handlers));

                if (instanceHandlerWifiEventAnyId == nullptr)
                    ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandlerWifiMarshaller, this, &instanceHandlerWifiEventAnyId), Wifi_Register_Event_Handlers_err, TAG, "register WIFI_EVENT::ESP_EVENT_ANY_ID failed");

                if (instanceHandlerIpEventGotIp == nullptr)
                    ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandlerWifiMarshaller, this, &instanceHandlerIpEventGotIp), Wifi_Register_Event_Handlers_err, TAG, "register IP_EVENT::IP_EVENT_STA_GOT_IP failed");

                wifiConnStep = WIFI_CONN::Set_Wifi_Mode;
                break;

            Wifi_Register_Event_Handlers_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Register_Handlers : error : " + esp_err_to_name(ret);
                wifiConnStep = WIFI_CONN::Error; // If we have any failures here, then exit out of our process.
                break;
            }

            case WIFI_CONN::Set_Wifi_Mode:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Set_Wifi_Mode - Step " + std::to_string((int)WIFI_CONN::Set_Wifi_Mode));

                ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), wifi_Set_Wifi_Mode_err, TAG, "esp_wifi_set_mode(WIFI_MODE_APSTA) failed");
                wifiConnStep = WIFI_CONN::Set_Sta_Config;
                break;

            wifi_Set_Wifi_Mode_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Set_Wifi_Mode : error : " + esp_err_to_name(ret);
                wifiConnStep = WIFI_CONN::Error; // If we have any failures here, then exit out of our process.
                break;
            }

            case WIFI_CONN::Set_Sta_Config:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Set_Sta_Config - Step " + std::to_string((int)WIFI_CONN::Set_Sta_Config));

                // Must do a read, modify, write with Config data.
                ESP_GOTO_ON_ERROR(esp_wifi_get_config(WIFI_IF_STA, &staConfig), wifi_Set_Sta_Config_err, TAG, "esp_wifi_get_config(WIFI_IF_STA, &staConfig) failed");

                // uint8_t ssid[32];                                            /**< SSID of target AP. */
                // uint8_t password[64];                                        /**< Password of target AP. */
                // wifi_scan_method_t scan_method;                              /**< do all channel scan or fast scan */
                // bool bssid_set;                                              /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.*/
                // uint8_t bssid[6];                                            /**< MAC address of target AP*/
                // uint8_t channel;                                             /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/
                // uint16_t listen_interval;                                    /**< Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0. */
                // wifi_sort_method_t sort_method;                              /**< sort the connect AP in the list by rssi or security mode */
                // wifi_scan_threshold_t threshold;                             /**< When sort_method is set, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used. */
                // wifi_pmf_config_t pmf_cfg;                                   /**< Configuration for Protected Management Frame. Will be advertised in RSN Capabilities in RSN IE. */
                // uint32_t rm_enabled : 1;                                     /**< Whether Radio Measurements are enabled for the connection */
                // uint32_t btm_enabled : 1;                                    /**< Whether BSS Transition Management is enabled for the connection */
                // uint32_t mbo_enabled : 1;                                    /**< Whether MBO is enabled for the connection */
                // uint32_t ft_enabled : 1;                                     /**< Whether FT is enabled for the connection */
                // uint32_t owe_enabled : 1;                                    /**< Whether OWE is enabled for the connection */
                // uint32_t transition_disable : 1;                             /**< Whether to enable transition disable feature */
                // uint32_t reserved : 26;                                      /**< Reserved for future feature set */
                // wifi_sae_pwe_method_t sae_pwe_h2e;                           /**< Configuration for SAE PWE derivation method */
                // wifi_sae_pk_mode_t sae_pk_mode;                              /**< Configuration for SAE-PK (Public Key) Authentication method */
                // uint8_t failure_retry_cnt;                                   /**< Number of connection retries station will do before moving to next AP. scan_method should be set as WIFI_ALL_CHANNEL_SCAN to use this staConfig.
                //                                                                   Note: Enabling this may cause connection time to increase incase best AP doesn't behave properly. */
                // uint32_t he_dcm_set : 1;                                     /**< Whether DCM max.constellation for transmission and reception is set. */
                // uint32_t he_dcm_max_constellation_tx : 2;                    /**< Indicate the max.constellation for DCM in TB PPDU the STA supported. 0: not supported. 1: BPSK, 2: QPSK, 3: 16-QAM. The default value is 3. */
                // uint32_t he_dcm_max_constellation_rx : 2;                    /**< Indicate the max.constellation for DCM in both Data field and HE-SIG-B field the STA supported. 0: not supported. 1: BPSK, 2: QPSK, 3: 16-QAM. The default value is 3. */
                // uint32_t he_mcs9_enabled : 1;                                /**< Whether to support HE-MCS 0 to 9. The default value is 0. */
                // uint32_t he_su_beamformee_disabled : 1;                      /**< Whether to disable support for operation as an SU beamformee. */
                // uint32_t he_trig_su_bmforming_feedback_disabled : 1;         /**< Whether to disable support the transmission of SU feedback in an HE TB sounding sequence. */
                // uint32_t he_trig_mu_bmforming_partial_feedback_disabled : 1; /**< Whether to disable support the transmission of partial-bandwidth MU feedback in an HE TB sounding sequence. */
                // uint32_t he_trig_cqi_feedback_disabled : 1;                  /**< Whether to disable support the transmission of CQI feedback in an HE TB sounding sequence. */
                // uint32_t he_reserved : 22;                                   /**< Reserved for future feature set */
                // uint8_t sae_h2e_identifier[SAE_H2E_IDENTIFIER_LEN];          /**< Password identifier for H2E. this needs to be null terminated string */

                memset(&staConfig.sta.ssid, 0, 32);     // Old ssid/pwd can be here prior to arrival,
                memset(&staConfig.sta.password, 0, 64); // so we always want to clear the entire array.

                if (hostStatus & _hostPriActive)
                {
                    memcpy(&staConfig.sta.ssid, (void *)ssidPri.c_str(), ssidPri.size());
                    memcpy(&staConfig.sta.password, (void *)ssidPwdPri.c_str(), ssidPwdPri.size());
                }

                // Authorization Mode - For quick reference, this is a list of my hosts and their authorzation modes
                // Hippo       4 WIFI_AUTH_WPA_WPA2_PSK
                // Wildcat     3 WIFI_AUTH_WPA2_PSK
                // Wildcat IOT 3 WIFI_AUTH_WPA2_PSK

                staConfig.sta.bssid_set = 0;
                // staConfig.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK; // Can flip from one to another for testing
                staConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                staConfig.sta.pmf_cfg.capable = true;
                staConfig.sta.pmf_cfg.required = false;

                if (showWifi & _showWifiConnSteps)
                {
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): sta.ssid                    " + std::string((char *)staConfig.sta.ssid));
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): sta.password                " + std::string((char *)staConfig.sta.password));
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): reserved                    " + std::to_string(staConfig.sta.reserved));
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): he_dcm_max_constellation_tx " + std::to_string(staConfig.sta.he_dcm_max_constellation_tx));
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): he_reserved                 " + std::to_string(staConfig.sta.he_reserved));
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): he_mcs9_enabled             " + std::to_string(staConfig.sta.he_mcs9_enabled));
                }

                ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &staConfig), wifi_Set_Sta_Config_err, TAG, "esp_wifi_set_config(WIFI_IF_STA, &staConfig) failed");
                wifiConnStep = WIFI_CONN::Wifi_Start;
                break;

            wifi_Set_Sta_Config_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Set_Sta_Config: error: " + esp_err_to_name(ret);
                wifiConnStep = WIFI_CONN::Error;
                break;
            }

            case WIFI_CONN::Wifi_Start:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_Start - Step " + std::to_string((int)WIFI_CONN::Wifi_Start));

                ESP_GOTO_ON_ERROR(esp_wifi_start(), wifi_Wifi_Start_err, TAG, "WIFI_CONN::Wifi_Start esp_wifi_start() failed");
                ESP_GOTO_ON_ERROR(esp_wifi_set_ps(WIFI_PS_MIN_MODEM), wifi_Wifi_Start_err, TAG, "WIFI_CONN::Wifi_Start esp_wifi_set_ps() failed");

                wifiHostTimeOut = false;                  // Reset all the flags and start the timer algorithm.
                wifiIPAddressTimeOut = false;             //
                wifiNoValidTimeTimeOut = false;           //
                wifiConnStartTicks = xTaskGetTickCount(); //

                if (showWifi & _showWifiConnSteps) // Announce our intent to Wait To Connect before we start the wait.  This reduces unneeded messaging in that wait state.
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_Waiting_To_Connect - Step " + std::to_string((int)WIFI_CONN::Wifi_Waiting_To_Connect));

                wifiConnStep = WIFI_CONN::Wifi_Waiting_To_Connect;
                break;

            wifi_Wifi_Start_err:
                errMsg = std::string(__func__) + "(): WIFI_CONN::Wifi_Start : error : " + esp_err_to_name(ret);
                wifiConnStep = WIFI_CONN::Error; // If we have any failures here, then exit out of our process.
                break;
            }

            case WIFI_CONN::Wifi_Waiting_To_Connect:
            {
                if (wifiConnState == WIFI_CONN_STATE::WIFI_CONNECTED_STA)
                {
                    wifiHostTimeOut = false;                  // Done looking for full STA Connection which includes receiving an IP address.
                    wifiConnStartTicks = xTaskGetTickCount(); // Restarting the timer to look for IP Address

                    if (showWifi & _showWifiConnSteps) // Announce our intent to Wait For IP Addres before we start the wait.  This reduces unneeded messaging in that wait state.
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_Waiting_For_IP_Address - Step " + std::to_string((int)WIFI_CONN::Wifi_Waiting_For_IP_Address));

                    wifiConnStep = WIFI_CONN::Wifi_Waiting_For_IP_Address;
                }
                else // We have not connected yet, so count time here.  If we time out waiting, then disconnect so the system can connect again.
                {
                    waitingOnHostConnSec = (int)(pdTICKS_TO_MS(xTaskGetTickCount() - wifiConnStartTicks) / 1000);

                    // ESP_LOGW(TAG, "waitingOnHostConnSec %d sec", waitingOnHostConnSec); // Debug

                    if ((noHostSecsToRestartMax - waitingOnHostConnSec) < 4) // Show a count down just before the time out.
                        ESP_LOGW(TAG, "Not Connected to Host, will restart in %d", noHostSecsToRestartMax - waitingOnHostConnSec);

                    if (waitingOnHostConnSec > noHostSecsToRestartMax)
                    {
                        wifiHostTimeOut = true;
                        wifiDiscStep = WIFI_DISC::Start; // Call for a disconnect process
                        wifiOP = WIFI_OP::Disconnect;
                        break;
                    }
                }
                break;
            }

            case WIFI_CONN::Wifi_Waiting_For_IP_Address:
            {
                if (haveIPAddress)
                {
                    wifiIPAddressTimeOut = false;             // Reset all the flags and start the timer algorithm.
                    wifiConnStartTicks = xTaskGetTickCount(); // Restarting timer to look for Epoch time.

                    wifiConnStep = WIFI_CONN::Wifi_SNTP_Connect;
                    break;
                }
                else // We have not received an IP Address yet, so count time here.  If we time out waiting, then disconnect so the system can connect again.
                {
                    waitingOnIPAddressSec = (int)(pdTICKS_TO_MS(xTaskGetTickCount() - wifiConnStartTicks) / 1000);

                    // ESP_LOGW(TAG, "waitingOnIPAddressSec %d sec", waitingOnIPAddressSec); // Debug

                    if ((noIPAddressSecToRestartMax - waitingOnIPAddressSec) < 4) // Show a count down just before the time out.
                        ESP_LOGW(TAG, "Don't have an IP address, will restart in %d", noIPAddressSecToRestartMax - waitingOnIPAddressSec);

                    if (waitingOnIPAddressSec > noIPAddressSecToRestartMax)
                    {
                        wifiIPAddressTimeOut = true;
                        wifiDiscStep = WIFI_DISC::Start; // Must always disconnect before connecting again.
                        wifiOP = WIFI_OP::Disconnect;
                        break;
                    }
                }
                break;
            }

            case WIFI_CONN::Wifi_SNTP_Connect:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_SNTP_Connect - Step " + std::to_string((int)WIFI_CONN::Wifi_SNTP_Connect));

                wifiConnStep = WIFI_CONN::Wifi_Waiting_SNTP_Valid_Time;

                if (showWifi & _showWifiConnSteps) // Techincally we are in the next state and we give an advanced notice here.
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Wifi_Waiting_SNTP_Valid_Time - Step " + std::to_string((int)WIFI_CONN::Wifi_Waiting_SNTP_Valid_Time));

                sntp->sntpOP = SNTP_OP::Connect;   // Ask SNTP to connect
                sntp->connStep = SNTP_CONN::Start; //
                sntp->run();                       // Enter the SNTP run loop
                break;
            }

            case WIFI_CONN::Wifi_Waiting_SNTP_Valid_Time:
            {
                if (sntp->timeValid)
                {
                    wifiNoValidTimeTimeOut = false;
                    wifiConnStep = WIFI_CONN::Finished;
                }
                else // We have not received Epoch time yet, so count seconds here.  If we time out waiting, then disconnect so the system can connect again.
                {
                    noValidTimeSecToRestart = (int)(pdTICKS_TO_MS(xTaskGetTickCount() - wifiConnStartTicks) / 1000);

                    // ESP_LOGW(TAG, "noValidTimeSecToRestart %d sec", noValidTimeSecToRestart); // Debug

                    if ((noValidTimeSecToRestartMax - noValidTimeSecToRestart) < 4) // Show a count down just before the time out.
                        ESP_LOGW(TAG, "Did not receive Epoch time, will restart in %d", noValidTimeSecToRestartMax - noValidTimeSecToRestart);

                    if (noValidTimeSecToRestart > noValidTimeSecToRestartMax)
                    {
                        wifiNoValidTimeTimeOut = true;
                        wifiDiscStep = WIFI_DISC::Start; // Must always disconnect before connecting again.
                        wifiOP = WIFI_OP::Disconnect;
                        break;
                    }

                    sntp->run(); // Make a another pass through SNTP for processing.
                }
                break;
            }

            case WIFI_CONN::Finished:
            {
                if (showWifi & _showWifiConnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_CONN::Finished");

                while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_CONNECTED), eSetValueWithoutOverwrite))
                    vTaskDelay(pdMS_TO_TICKS(50));

                cadenceTimeDelay = 250; // Return to relaxed scheduling.
                wifiOP = WIFI_OP::Directives;
                break;
            }

            case WIFI_CONN::Error:
            {
                wifiConnStep = WIFI_CONN::Finished; // Park this state
                wifiOP = WIFI_OP::Error;            // Vector to error handling
                break;
            }
            }
            break;
        }

        case WIFI_OP::Disconnect:
        {
            switch (wifiDiscStep)
            {
            case WIFI_DISC::Start:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Start");

                cadenceTimeDelay = 0; // Don't permit scheduler delays in Run processing.
                wifiDiscStep = WIFI_DISC::Cancel_Connect;
                [[fallthrough]];
            }

            case WIFI_DISC::Cancel_Connect:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Cancel_Connect - Step " + std::to_string((int)WIFI_DISC::Cancel_Connect));

                wifiDiscStep = WIFI_DISC::Deinitialize_SNTP; // Next step by default.

                if (wifiConnStep != WIFI_CONN::Finished)
                {
                    //
                    // If we call for a Disconnect in the middle of a Connection process, we need to reverse the process
                    // starting at just the right step so we can be sure to deallocate resources.
                    //
                    if (wifiConnStep >= WIFI_CONN::Wifi_Waiting_SNTP_Valid_Time)
                        wifiDiscStep = WIFI_DISC::Deinitialize_SNTP;
                    else if (wifiConnStep == WIFI_CONN::Wifi_SNTP_Connect)
                        wifiDiscStep = WIFI_DISC::Wifi_Disconnect;
                    else if (wifiConnStep >= WIFI_CONN::Set_Wifi_Mode)
                        wifiDiscStep = WIFI_DISC::Wifi_Disconnect;
                    else if (wifiConnStep >= WIFI_CONN::Register_Handlers)
                        wifiDiscStep = WIFI_DISC::Unregister_Handlers;
                    else if (wifiConnStep >= WIFI_CONN::Wifi_Init)
                        wifiDiscStep = WIFI_DISC::Wifi_Deinit;
                    else if (wifiConnStep >= WIFI_CONN::Create_Netif_Objects)
                        wifiDiscStep = WIFI_DISC::Destroy_Netif_Objects;

                    wifiConnStep = WIFI_CONN::Finished; // Make sure the the connection process is canceled.
                }

                break;
            }

            case WIFI_DISC::Deinitialize_SNTP:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Deinitialize_SNTP - Step " + std::to_string((int)WIFI_DISC::Deinitialize_SNTP));

                esp_sntp_stop();
                esp_netif_sntp_deinit(); // Deinitialize esp_netif SNTP module inside the IDF.

                wifiDiscStep = WIFI_DISC::Wifi_Disconnect;
                break;
            }

            case WIFI_DISC::Wifi_Disconnect:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Wifi_Disconnect - Step " + std::to_string((int)WIFI_DISC::Wifi_Disconnect));

                sntp->connStep = SNTP_CONN::Idle; // Reset the states
                sntp->sntpOP = SNTP_OP::Idle;

                ret = esp_wifi_disconnect();

                if (ret == ESP_OK)
                {
                    ret = esp_wifi_stop();

                    if (ret == ESP_OK)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): esp_wifi_stop() suceeded");
                    else if (ret == ESP_ERR_WIFI_NOT_INIT)
                        routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): WiFi is not initialized by esp_wifi_init - esp_wifi_stop()");
                    else // Unknown error
                    {
                        errMsg = std::string(__func__) + "(): WIFI_DISC::Wifi_Disconnect: esp_wifi_stop() error : " + esp_err_to_name(ret);
                        wifiDiscStep = WIFI_DISC::Error;
                        break;
                    }
                }
                else if (ret == ESP_ERR_WIFI_NOT_INIT)
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): WiFi is not initialized by esp_wifi_init - esp_wifi_disconnect()");
                else if (ret == ESP_ERR_WIFI_NOT_STARTED)
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): WiFi is not started by esp_wifi_start - esp_wifi_disconnect()");
                else // Unknown error
                {
                    errMsg = std::string(__func__) + "(): WIFI_DISC::Wifi_Disconnect: esp_wifi_disconnect() error : " + esp_err_to_name(ret);
                    wifiDiscStep = WIFI_DISC::Error;
                    break;
                }

                wifiConnState = WIFI_CONN_STATE::WIFI_DISCONNECTED; // Secondary assignment here just in case the Disconnect event doesn't fire as expected.
                wifiDiscStep = WIFI_DISC::Reset_Flags;
                break;
            }

            case WIFI_DISC::Reset_Flags:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Reset_Flags - Step " + std::to_string((int)WIFI_DISC::Reset_Flags));

                sntp->timeValid = false;                       // SNTP Time is not valid
                haveIPAddress = false;                         // We don't have an IP Address
                wifiDiscStep = WIFI_DISC::Unregister_Handlers; //
                break;
            }

            case WIFI_DISC::Unregister_Handlers:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Unregister_Handlers - Step " + std::to_string((int)WIFI_DISC::Unregister_Handlers));

                if (instanceHandlerWifiEventAnyId != nullptr)
                {
                    ESP_GOTO_ON_ERROR(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instanceHandlerWifiEventAnyId), wifi_Unregister_Handlers_err, TAG, "WIFI_DISC::Unregister_Handlers failed");
                    instanceHandlerWifiEventAnyId = nullptr;
                }

                if (instanceHandlerIpEventGotIp != nullptr)
                {
                    ESP_GOTO_ON_ERROR(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instanceHandlerIpEventGotIp), wifi_Unregister_Handlers_err, TAG, "WIFI_DISC::Unregister_Handlers failed");
                    instanceHandlerIpEventGotIp = nullptr;
                }

                wifiDiscStep = WIFI_DISC::Wifi_Deinit;
                break;

            wifi_Unregister_Handlers_err:
                errMsg = std::string(__func__) + "(): WIFI_DISC::esp_event_handler_instance_unregister() error : " + esp_err_to_name(ret);
                wifiDiscStep = WIFI_DISC::Error; // If we have any failures here, then exit out of our process.
                break;
            }

            case WIFI_DISC::Wifi_Deinit:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Wifi_Deinit - Step " + std::to_string((int)WIFI_DISC::Wifi_Deinit));

                ESP_GOTO_ON_ERROR(esp_wifi_deinit(), wifi_Wifi_Deinit_err, TAG, "esp_wifi_deinit() failed");
                wifiDiscStep = WIFI_DISC::Destroy_Netif_Objects;
                break;

            wifi_Wifi_Deinit_err:
                errMsg = std::string(__func__) + "(): WIFI_DISC::Wifi_Deinit error : " + esp_err_to_name(ret);
                wifiDiscStep = WIFI_DISC::Error; // If we have any failures here, then exit out of our process.
                break;
            }

            case WIFI_DISC::Destroy_Netif_Objects:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Destroy_Netif_Objects - Step " + std::to_string((int)WIFI_DISC::Destroy_Netif_Objects));

                if (defaultSTANetif != nullptr)
                {
                    esp_netif_destroy_default_wifi(defaultSTANetif);
                    defaultSTANetif = nullptr;
                }

                wifiDiscStep = WIFI_DISC::Finished;
                break;
            }

            case WIFI_DISC::Finished:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Finished");

                cadenceTimeDelay = 250; // Return to relaxed scheduling.

                // This could potientially be a secondary call back to the System to annouce a disconnection.
                while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_DISCONNECTED), eSetValueWithoutOverwrite))
                    vTaskDelay(pdMS_TO_TICKS(50));

                if (wifiShdnStep != WIFI_SHUTDOWN::Finished) // Give priority to a call for Shut Down
                {
                    if (showWifi & _showWifiDiscSteps)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Returning to Shutdown...");

                    wifiHostTimeOut = false;
                    wifiIPAddressTimeOut = false;
                    wifiNoValidTimeTimeOut = false;
                    wifiOP = WIFI_OP::Shutdown;
                    break;
                }

                // Favor running the directives. There are two reasons that we may return to directives:
                // 1) We may be provisioning a host.  During provisioning, we shut down the Wifi if currently connected.
                // 2) In the future we will implement the option of rotating over to a secondary network.  And this may be commanded from directives.
                if (wifiDirectivesStep != WIFI_DIRECTIVES::Finished)
                {
                    wifiOP = WIFI_OP::Directives; // Make plans to return to Directives it if is not in a Finished state.
                    break;
                }

                if (wifiHostTimeOut || wifiIPAddressTimeOut || wifiNoValidTimeTimeOut) // Are we servicing a Host or SNTP timeout?
                {
                    wifiConnStep = WIFI_CONN::Start; // We always try to start again after a disconnection that is not commanded.
                    wifiOP = WIFI_OP::Connect;       //
                }
                break;
            }

            case WIFI_DISC::Error:
            {
                if (showWifi & _showWifiDiscSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_DISC::Error");
                wifiConnStep = WIFI_CONN::Finished;
                wifiOP = WIFI_OP::Error;
                break;
            }
            }
            break;
        }

        case WIFI_OP::Provision:
        {
            switch (wifiProvStep)
            {
            case WIFI_PROV::Start:
            {
                if (showWifi & _showWifiProvSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_PROV::Start");

                if (showWifi & _showWifiProvSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_PROV::Wait_On_Provision - Step " + std::to_string((int)WIFI_PROV::Wait_On_Provision));

                prov = new PROV(queueCmdRequests);
                if (prov != nullptr)
                    taskHandleProvisionRun = prov->taskHandleRun;
                wifiProvStep = WIFI_PROV::Wait_On_Provision;
                break;
            }

            case WIFI_PROV::Wait_On_Provision:
            {
                if (xSemaphoreTake(semProvEntry, 100))
                {
                    delete prov;
                    prov = nullptr; // Destructor will not set pointer null.  We have to do that manually.

                    if (showWifi & _showWifiProvSteps)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): prov deleted");
                    wifiProvStep = WIFI_PROV::Finished;
                }
                break;
            }

            case WIFI_PROV::Finished:
            {
                if (showWifi & _showWifiProvSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_PROV::Finished");

                wifiOP = WIFI_OP::Directives;
                break;
            }

            case WIFI_PROV::Error:
            {
                if (showWifi & _showWifiProvSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_PROV::Error");
            }
            }
            break;
        }

        case WIFI_OP::Error:
        {
            cadenceTimeDelay = 250; // Return to relaxed scheduling.

            while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_DISCONNECTED), eSetValueWithoutOverwrite))
                vTaskDelay(pdMS_TO_TICKS(50));

            routeLogByValue(LOG_TYPE::ERROR, errMsg);
            wifiOP = WIFI_OP::Idle;
            [[fallthrough]];
        }

        case WIFI_OP::Idle:
        {
            cadenceTimeDelay = 250; // Return to relaxed scheduling.
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        }
    }
}

//
// This is where we execute event handler actions.  Removing all this activity from the event handler eliminates task contention
// over variables and the system event task can get back quickly to other more important event processing activities.
//
void Wifi::runEvents()
{
    esp_err_t ret = ESP_OK;
    WIFI_Event evt;

    while (xQueueReceive(queueEvents, &evt, 0)) // Process all events in the queue
    {
        if (show & _showEvents)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): evt.event_base is " + std::string(evt.event_base) + " evt.event_id is " + std::to_string(evt.event_id));

        if (evt.event_base == WIFI_EVENT)
        {
            switch (evt.event_id)
            {
            case WIFI_EVENT_SCAN_DONE:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_SCAN_DONE");
                break;
            }

            case WIFI_EVENT_STA_START:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_STA_START - wifi connect");

                if (wifiConnState != WIFI_CONN_STATE::WIFI_READY_TO_CONNECT)
                {
                    wifiConnState = WIFI_CONN_STATE::WIFI_READY_TO_CONNECT;

                    if (show & _showEvents)
                        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): wifiConnState = WIFI_READY_TO_CONNECT");
                }

                haveIPAddress = false;   // Can't possible have an IP address if we are not connected.
                sntp->timeValid = false; // Everytime we start the STA Connection process, the SNTP time must be declared invalid.

                ESP_GOTO_ON_ERROR(esp_wifi_connect(), wifi_eventRun_err, TAG, "esp_wifi_connect() failed");

                wifiConnState = WIFI_CONN_STATE::WIFI_CONNECTING_STA;

                while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_CONNECTING), eSetValueWithoutOverwrite))
                    vTaskDelay(pdMS_TO_TICKS(50));
                break;
            }

            case WIFI_EVENT_STA_STOP:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_STA_STOP");

                sntp->timeValid = false; // When we stop the sta connection, sntp time must be invalidated.

                if (wifiConnState != WIFI_CONN_STATE::WIFI_DISCONNECTED)
                    wifiConnState = WIFI_CONN_STATE::WIFI_DISCONNECTING_STA; // Only run these items one time at a Disconnection.

                while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_DISCONNECTING), eSetValueWithoutOverwrite))
                    vTaskDelay(pdMS_TO_TICKS(50));
                break;
            }

            case WIFI_EVENT_STA_CONNECTED:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_STA_CONNECTED - IP address will follow...");
                //
                // We have connected to the Host.   This doesn't mean that we have an IP or access to the Internet yet,
                // but we can confirm the ssid and password are valid for the targeted host.
                //
                wifiConnState = WIFI_CONN_STATE::WIFI_CONNECTED_STA;
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_STA_DISCONNECTED");

                sntp->timeValid = false; // When we disconnect, sntp time must be invalidated.

                if (wifiConnState != WIFI_CONN_STATE::WIFI_DISCONNECTED) // The first time we disconnect, set the state and send any required notifications.
                    wifiConnState = WIFI_CONN_STATE::WIFI_DISCONNECTED;

                while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_DISCONNECTED), eSetValueWithoutOverwrite))
                    vTaskDelay(pdMS_TO_TICKS(50));

                if (autoConnect)
                {
                    haveIPAddress = false;
                    sntp->timeValid = false;

                    ESP_GOTO_ON_ERROR(esp_wifi_connect(), wifi_eventRun_err, TAG, "esp_wifi_connect() failed"); // Try to reconnect (But don't reset our timeout counts)
                    wifiConnState = WIFI_CONN_STATE::WIFI_CONNECTING_STA;

                    while (!xTaskNotify(taskHandleSystemRun, static_cast<uint32_t>(SYS_NOTIFY::NFY_WIFI_CONNECTING), eSetValueWithoutOverwrite))
                        vTaskDelay(pdMS_TO_TICKS(50));
                }
                break;
            }

            case WIFI_EVENT_AP_START:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_AP_START");

                break;
            }

            case WIFI_EVENT_AP_STOP:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_AP_STOP");

                break;
            }

            case WIFI_EVENT_AP_STACONNECTED:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_AP_STACONNECTED");

                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_AP_STADISCONNECTED");

                break;
            }

            case WIFI_EVENT_STA_BEACON_TIMEOUT:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_STA_BEACON_TIMEOUT, Starting to lose a connection...");

                break;
            }

            case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): WIFI_EVENT:WIFI_EVENT_HOME_CHANNEL_CHANGE");

                break;
            }

            default:
            {
                routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): WIFI_EVENT:<default> event_id = " + std::to_string(evt.event_id));
                break;
            }
            }
        }
        else if (evt.event_base == IP_EVENT)
        {
            switch (evt.event_id)
            {
            case IP_EVENT_STA_GOT_IP: // ESP32 station got IP from connected AP
            {
                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IP_EVENT:IP_EVENT_STA_GOT_IP:");

                esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                ESP_GOTO_ON_FALSE(netif, ESP_FAIL, wifi_eventRun_err, TAG, "Could not locate the WIFI_STA_DEF netif instance..."); // Effectively, this is an assert without an abort.

                esp_netif_ip_info_t ip_info;
                ESP_GOTO_ON_ERROR(esp_netif_get_ip_info(netif, &ip_info), wifi_eventRun_err, TAG, "esp_netif_get_ip_info() failed ");

                uint8_t num1 = ((&(ip_info.ip.addr))[0]);
                uint8_t num2 = ((&(ip_info.ip.addr))[1]);
                uint8_t num3 = ((&(ip_info.ip.addr))[2]);
                uint8_t num4 = ((&(ip_info.ip.addr))[3]);

                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IP_EVENT:IP_EVENT_STA_GOT_IP: My IP address " + std::to_string(num1) + "." + std::to_string(num2) + "." + std::to_string(num3) + "." + std::to_string(num4));

                num1 = ((&(ip_info.gw.addr))[0]);
                num2 = ((&(ip_info.gw.addr))[1]);
                num3 = ((&(ip_info.gw.addr))[2]);
                num4 = ((&(ip_info.gw.addr))[3]);

                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IP_EVENT:IP_EVENT_STA_GOT_IP: My Gateway address " + std::to_string(num1) + "." + std::to_string(num2) + "." + std::to_string(num3) + "." + std::to_string(num4));

                num1 = ((&(ip_info.netmask.addr))[0]);
                num2 = ((&(ip_info.netmask.addr))[1]);
                num3 = ((&(ip_info.netmask.addr))[2]);
                num4 = ((&(ip_info.netmask.addr))[3]);

                if (show & _showEvents)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IP_EVENT:IP_EVENT_STA_GOT_IP: My Netmask is " + std::to_string(num1) + "." + std::to_string(num2) + "." + std::to_string(num3) + "." + std::to_string(num4));

                haveIPAddress = true; // This will stop the wifi waiting process.

                break;
            }

            default:
            {
                routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "():IP_EVENT:<default> event_id = " + std::to_string(evt.event_id));
                break;
            }
            }
        }
    }
    return;

wifi_eventRun_err:
    errMsg = std::string(__func__) + "(): error: " + esp_err_to_name(ret);
    wifiOP = WIFI_OP::Error;
}
