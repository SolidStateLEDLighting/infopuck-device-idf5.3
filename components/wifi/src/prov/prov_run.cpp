#include "prov/prov_.hpp"
#include "system_.hpp"

#include "wifi/wifi_enums.hpp"

#include "esp_check.h"

/* Local Semaphores */
extern SemaphoreHandle_t semProvEntry;
extern SemaphoreHandle_t semProvRouteLock;

void PROV::runMarshaller(void *arg)
{
    ((PROV *)arg)->run();
    ((PROV *)arg)->taskHandleRun = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
    vTaskDelete(NULL);
}

void PROV::run(void)
{
    esp_err_t ret = ESP_OK;
    std::string serverName = "";

    PROV_NOTIFY provTaskNotifyValue = static_cast<PROV_NOTIFY>(0);

    while (true)
    {
        if (uxQueueMessagesWaiting(queueEvents)) // We always give top priorty to handling events
            runEvents();

        provTaskNotifyValue = static_cast<PROV_NOTIFY>(ulTaskNotifyTake(pdTRUE, 20));

        if (provTaskNotifyValue > static_cast<PROV_NOTIFY>(0)) // Looking for Task Notifications
        {
            // Task Notifications should be used for notifications (NFY_NOTIFICATION) or commands (CMD_COMMAND) both of which need no input and return no data.
            switch (provTaskNotifyValue)
            {
            case PROV_NOTIFY::CMD_PRINT_TASK_INFO: // Some of these notifications set Directive bits - a follow up CMD_RUN_DIRECTIVES task notification starts the action.
            {
                if (show & _showRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): Received PROV_NOTIFY::CMD_PRINT_TASK_INFO");
                printTaskInfoByColumns(NULL);
                break;
            }

            case PROV_NOTIFY::CMD_LOG_TASK_INFO:
            {
                if (show & _showRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): Received PROV_NOTIFY::CMD_LOG_TASK_INFO");
                logTaskInfo(semProvRouteLock, TAG);
                break;
            }
            }
        }

        switch (provOP)
        {
        case PROV_OP::Run:
        {
            // Order of operations used for provisioning
            //
            // esp_event_handler_instance_register();
            // esp_netif_create_default_wifi_sta();
            // esp_netif_create_default_wifi_ap();
            //
            // esp_wifi_init();
            // esp_wifi_set_mode();
            // esp_wifi_start();
            //
            // wifi_prov_mgr_init();
            // wifi_prov_mgr_endpoint_create();
            // wifi_prov_mgr_start_provisioning();
            // wifi_prov_mgr_endpoint_register();
            //
            // <provisioning activity happens here>
            //
            // wifi_prov_mgr_endpoint_unregister();
            // wifi_prov_mgr_stop_provisioning();
            // wifi_prov_mgr_deinit();
            //
            // esp_wifi_restore();
            // esp_wifi_stop();
            // esp_wifi_deinit();
            //
            // esp_netif_destroy_default_wifi();
            // esp_event_handler_instance_unregister();
            //
            switch (provRunStep)
            {
            case PROV_RUN::Start:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start");

                blnCancelProvisioning = false;
                provRunStep = PROV_RUN::Register_Event_Handlers;
                [[fallthrough]];
            }

            case PROV_RUN::Register_Event_Handlers:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Register_Event_Handlers - Step " + std::to_string((int)PROV_RUN::Register_Event_Handlers));

                ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, eventHandlerProvisionMarshaller, this, &instanceHndProvEvents), prov_Run_err, TAG, "Register WIFI_PROV_EVENT:ESP_EVENT_ANY_ID  failed");
                ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, eventHandlerProvisionMarshaller, this, &instanceHndSessionEvents), prov_Run_err, TAG, "Register PROTOCOMM_SECURITY_SESSION_EVENT::ESP_EVENT_ANY_ID failed");
                provRunStep = PROV_RUN::Create_Netif_Objects;
                break;
            }

            case PROV_RUN::Create_Netif_Objects:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Create_Netif_Objects - Step " + std::to_string((int)PROV_RUN::Create_Netif_Objects));

                defaultSTANetif = esp_netif_create_default_wifi_sta();

                if (defaultSTANetif == nullptr)
                {
                    errMsg = std::string(__func__) + "(): PROV_RUN::Create_Netif_Objects: esp_netif_create_default_wifi_sta() failed";
                    provRunStep = PROV_RUN::Error;
                    break;
                }

                defaultSAPNetif = esp_netif_create_default_wifi_ap();

                if (defaultSAPNetif == nullptr)
                {
                    errMsg = std::string(__func__) + "(): PROV_RUN::Create_Netif_Objects : esp_netif_create_default_wifi_ap() failed.";
                    provRunStep = PROV_RUN::Error;
                    break;
                }

                provRunStep = PROV_RUN::Wifi_Init;
                break;
            }

            case PROV_RUN::Wifi_Init:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Wifi_Init - Step " + std::to_string((int)PROV_RUN::Wifi_Init));

                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
                ESP_GOTO_ON_ERROR(esp_wifi_init(&cfg), prov_Run_err, TAG, "PROV_RUN::Wifi_Init: esp_wifi_init() failure.");
                provRunStep = PROV_RUN::Set_Mode;
                break;
            }

            case PROV_RUN::Set_Mode:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Set_Mode - Step " + std::to_string((int)PROV_RUN::Set_Mode));

                ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), prov_Run_err, TAG, "PROV_RUN::Set_Mode: esp_wifi_set_mode() failure.");
                ESP_GOTO_ON_ERROR(esp_wifi_start(), prov_Run_err, TAG, "PROV_RUN::Set_Mode: esp_wifi_start() failure.");
                provRunStep = PROV_RUN::Init_Provisioning_Manager;
                break;
            }

            case PROV_RUN::Init_Provisioning_Manager:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Init_Provisioning_Manager - Step " + std::to_string((int)PROV_RUN::Init_Provisioning_Manager));
                //
                // I tried to discover how to re-route events to another event loop, but could not figure that out.  By default all
                // events route to the default event loop, but it might be wiser in the future to create a custom event loop and route
                // provision events there.  I was able to do everything except for configuring the prov manager to send events to
                // a custom event loop.  I do know how to send all WIFI_PROV_EVENT events to a handler (there is an example of this
                // available), but I don't know how to also catch the PROTOCOMM_TRANSPORT_BLE_EVENT and PROTOCOMM_SECURITY_SESSION_EVENT
                // events unless I do it throught the default event handler.
                //
                config.scheme = wifi_prov_scheme_softap;
                config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;

                config.app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE; // Must assign this or app will crash!

                ESP_GOTO_ON_ERROR(wifi_prov_mgr_init(config), prov_Run_err, TAG, "wifi_prov_mgr_init() failure.");

                provRunStep = PROV_RUN::Start_Provisioning;
                break;
            }

            case PROV_RUN::Start_Provisioning:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning - Step " + std::to_string((int)PROV_RUN::Start_Provisioning));

                // The service_key is the Wifi password when scheme is wifi_prov_scheme_softap.  (Minimum expected length: 8, maximum 64 for WPA2-PSK)
                // Ignored when scheme is wifi_prov_scheme_ble
                const char *service_key = NULL;

                // What is the Device Service Name that we want
                // - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
                // - Device name when scheme is wifi_prov_scheme_ble
                char service_name[12];
                getDeviceServiceName(service_name, sizeof(service_name));

                logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning: Service name is " + std::string(service_name));

                if (blnSecurityVersion1)
                {
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning: Security Version 1");
                    security = WIFI_PROV_SECURITY_1;

                    userName = "";
                    sec_params = pop.c_str();
                }
                else if (blnSecurityVersion2)
                {
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning: Security Version 2");
                    security = WIFI_PROV_SECURITY_2;

                    if (blnSec2ProductionMode)
                    {
                        userName = ""; // Clear out the development userName/pop that are set by default.
                        pop = "";
                    }
                    //
                    // This is the structure for passing security parameters for the protocomm security 2.
                    // If dynamically allocated, sec2_params pointer and its content must be valid till WIFI_PROV_END event is triggered.
                    ESP_GOTO_ON_ERROR(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len), prov_Run_err, TAG, "example_get_sec2_salt() failed");
                    ESP_GOTO_ON_ERROR(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len), prov_Run_err, TAG, "example_get_sec2_verifier() failed");
                    sec_params = (wifi_prov_security1_params_t *)&sec2_params;
                }

                // An optional endpoint that applications can create if they expect to get some additional custom data during provisioning workflow.
                // The endpoint name can be anything of your choice.  This call must be made before starting the provisioning.
                ESP_GOTO_ON_ERROR(wifi_prov_mgr_endpoint_create("custom-data"), prov_Run_err, TAG, "wifi_prov_mgr_endpoint_create() failed");

                if (blnAllowReprovisioning)
                    ESP_GOTO_ON_ERROR(wifi_prov_mgr_disable_auto_stop(100), prov_Run_err, TAG, "wifi_prov_mgr_disable_auto_stop() failed");

                logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning: Ready to wifi_prov_mgr_start_provisioning...");

                ESP_GOTO_ON_ERROR(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, service_key), prov_Run_err, TAG, "wifi_prov_mgr_start_provisioning() failed");

                // The handler for the optional endpoint created above.  This call must be made after starting the provisioning, and only if the endpoint has already been created above.
                ESP_GOTO_ON_ERROR(wifi_prov_mgr_endpoint_register("custom-data", eventHandlerCustomMarshaller, this), prov_Run_err, TAG, "wifi_prov_mgr_endpoint_register() failed");

                // logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Start_Provisioning : Ready to generate QR code...");
                // printQRCode(service_name, userName.c_str(), pop.c_str(), PROV_TRANSPORT_SOFTAP); // Print QR code for provisioning

                provRunStep = PROV_RUN::Process_Wait; // Let event handling take the next action.
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Process_Wait - Step " + std::to_string((int)PROV_RUN::Process_Wait));
                break;
            }

            case PROV_RUN::Process_Wait:
            {
                if (blnCancelProvisioning)
                    provRunStep = PROV_RUN::Unregister_Endpoints;
                else
                    provOP = PROV_OP::Wait;
                break;
            }

            case PROV_RUN::Unregister_Endpoints:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Unregister_Endpoints - Step " + std::to_string((int)PROV_RUN::Unregister_Endpoints));

                wifi_prov_mgr_endpoint_unregister("custom-data");
                provRunStep = PROV_RUN::Stop;
                break;
            }

            case PROV_RUN::Stop:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Stop - Step " + std::to_string((int)PROV_RUN::Stop));

                wifi_prov_mgr_stop_provisioning();

                provRunStep = PROV_RUN::Deinitialize;
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Stop_Wait - Step " + std::to_string((int)PROV_RUN::Stop_Wait));
                break;
            }

            case PROV_RUN::Stop_Wait:
            {
                if (blnCancelProvisioning)
                    provRunStep = PROV_RUN::Deinitialize;
                else
                    provOP = PROV_OP::Wait;
                break;
            }

            case PROV_RUN::Deinitialize:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Deinitialize - Step " + std::to_string((int)PROV_RUN::Deinitialize));

                wifi_prov_mgr_deinit();

                provRunStep = PROV_RUN::Deinitalization_Wait;
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Deinitalization_Wait - Step " + std::to_string((int)PROV_RUN::Deinitalization_Wait));
                break;
            }

            case PROV_RUN::Deinitalization_Wait:
            {
                if (blnCancelProvisioning)
                    provRunStep = PROV_RUN::Stop_Destroy_Unregister;
                else
                    provOP = PROV_OP::Wait;
                break;
            }

            case PROV_RUN::Stop_Destroy_Unregister:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Stop_Destroy_Unregister - Step " + std::to_string((int)PROV_RUN::Stop_Destroy_Unregister));

                ESP_GOTO_ON_ERROR(esp_wifi_restore(), prov_Run_err, TAG, "PROV_RUN::prov_Stop_Destroy_Unregister_err: esp_wifi_restore() error");
                ESP_GOTO_ON_ERROR(esp_wifi_stop(), prov_Run_err, TAG, "PROV_RUN::prov_Stop_Destroy_Unregister_err: esp_wifi_stop() failed");

                if (defaultSTANetif != nullptr)
                {
                    esp_netif_destroy_default_wifi(defaultSTANetif);
                    defaultSTANetif = nullptr;
                }

                if (defaultSAPNetif != nullptr)
                {
                    esp_netif_destroy_default_wifi(defaultSAPNetif);
                    defaultSAPNetif = nullptr;
                }

                ESP_GOTO_ON_ERROR(esp_wifi_deinit(), prov_Run_err, TAG, "esp_wifi_deinit() failure.");
                ESP_GOTO_ON_ERROR(esp_event_handler_instance_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, instanceHndProvEvents), prov_Run_err, TAG, "esp_event_handler_instance_unregister_with() failed");
                ESP_GOTO_ON_ERROR(esp_event_handler_instance_unregister(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, instanceHndSessionEvents), prov_Run_err, TAG, "esp_event_handler_instance_unregister_with() failed");

                provRunStep = PROV_RUN::Finished;
                break;
            }

            case PROV_RUN::Finished:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Finished");

                // printTaskInfoByColumns();

                if (semProvEntry != NULL)
                    xSemaphoreGive(semProvEntry); // Allow the calling object to continue.
                provOP = PROV_OP::Idle;
                break;
            }

            case PROV_RUN::Error:
            {
                if (showPROV & _showProvRun)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROV_RUN::Error");

                provOP = PROV_OP::Error;
                provRunStep = PROV_RUN::Finished;
                break;
            }

            case PROV_RUN::Idle: // During any provisioning, the process ends up idle here (unless an error occurs).
            {
                if (blnCancelProvisioning)
                    return;

                vTaskDelay(5000);
                break;
            }
            }
            break;

        prov_Run_err:
            errMsg = std::string(__func__) + "(): error : " + esp_err_to_name(ret);
            provRunStep = PROV_RUN::Idle;
            provOP = PROV_OP::Error; // If we have any failures here, then exit out of our process.
            break;
        }

        case PROV_OP::Wait:
        {
            taskYIELD();
            provOP = PROV_OP::Run;
            break;
        }

        case PROV_OP::Error:
        {
            logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, errMsg);
            provOP = PROV_OP::Idle;
            break;
        }

        case PROV_OP::Idle:
        {
            if (blnCancelProvisioning)
                return;
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        }
    }
}

void PROV::runEvents()
{
    esp_err_t ret = ESP_OK;
    PROV_Event evt;

    while (xQueueReceive(queueEvents, &evt, 0)) // Process all events in the queue
    {
        if (show & _showEvents)
            logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): evt.event_base is " + std::string(evt.event_base) + " evt.event_id is " + std::to_string(evt.event_id));

        // ESP_LOGW(TAG, "queueEvents size is %d", uxQueueMessagesWaiting(queueEvents));

        if (evt.event_base == WIFI_PROV_EVENT)
        {
            switch (evt.event_id)
            {
            case WIFI_PROV_INIT:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_INIT:");
                break;
            }

            case WIFI_PROV_START:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_START:");
                break;
            }

            case WIFI_PROV_CRED_RECV:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_CRED_RECV:");

                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)evt.data; // Copy the data
                ssidUnderTest = std::string((const char *)wifi_sta_cfg->ssid);
                ssidPwdUnderTest = std::string((const char *)wifi_sta_cfg->password);
                break;
            }

            case WIFI_PROV_CRED_FAIL:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_WARN, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_CRED_FAIL");

                ESP_LOGW(TAG, "WIFI_PROV_CRED_FAIL provOP/provRunStep %d/%d", (int)provOP, (int)provRunStep);

                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)evt.data;

                if (*reason == WIFI_PROV_STA_AUTH_ERROR)
                    logByValue(ESP_LOG_WARN, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_CRED_FAIL: Station Authentication failed.");
                else
                    logByValue(ESP_LOG_WARN, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_CRED_FAIL: Wi-Fi Access-Point not found.");

                if (blnResetProvMgrOnFailure)
                {
                    provFailureRetries++;
                    if (provFailureRetries >= maxFailureRetryCount)
                    {
                        ESP_GOTO_ON_ERROR(wifi_prov_mgr_reset_sm_state_on_failure(), prov_WIFI_PROV_CRED_FAIL_err, TAG, "wifi_prov_mgr_reset_sm_state_on_failure() failed");
                        provFailureRetries = 0;
                    }
                }
                break;

            prov_WIFI_PROV_CRED_FAIL_err:
                logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_CRED_FAIL:Error " + std::to_string(ret) + " " + esp_err_to_name(ret));
                break;
            }

            case WIFI_PROV_CRED_SUCCESS:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_CRED_SUCCESS");

                WIFI_CmdRequest *wifiCmdRequest = new WIFI_CmdRequest; // This is local and we hand over its address like a pointer
                wifiCmdRequest->requestedCmd = WIFI_COMMAND::SET_SSID_PRI;
                memset(&wifiCmdRequest->data1, 0, 32);
                memcpy(&wifiCmdRequest->data1, ssidUnderTest.c_str(), ssidUnderTest.size());
                wifiCmdRequest->data1Length = ssidUnderTest.size();

                memset(&wifiCmdRequest->data2, 0, 64);
                memcpy(&wifiCmdRequest->data2, ssidPwdUnderTest.c_str(), ssidPwdUnderTest.size());
                wifiCmdRequest->data2Length = ssidPwdUnderTest.size();

                while (uxQueueMessagesWaiting(wifiQueCmdRequests) > 0) // Wait here until we are sure the Mailbox Queue is clear
                    vTaskDelay(pdMS_TO_TICKS(10));

                xQueueSend(wifiQueCmdRequests, (void *)&wifiCmdRequest, portMAX_DELAY); // Send the Request

                while (uxQueueMessagesWaiting(wifiQueCmdRequests) > 0) // Wait must here until we are sure the Mailbox Queue is clear If we don't wait,
                    vTaskDelay(pdMS_TO_TICKS(100));                    // this side destroys the Request before other side has a chance to read it.

                delete wifiCmdRequest;
                break;
            }

            case WIFI_PROV_END:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_END");

                provRunStep = PROV_RUN::Unregister_Endpoints;
                provOP = PROV_OP::Run;
                break;
            }

            case WIFI_PROV_DEINIT:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:WIFI_PROV_DEINIT");

                provRunStep = PROV_RUN::Stop_Destroy_Unregister;
                provOP = PROV_OP::Run;
                break;
            }

            default:
            {
                logByValue(ESP_LOG_WARN, semProvRouteLock, TAG, std::string(__func__) + "(): WIFI_PROV_EVENT:<default> event_id = " + std::to_string(evt.event_id));
                break;
            }
            }
        }
        else if (evt.event_base == PROTOCOMM_SECURITY_SESSION_EVENT)
        {
            switch (evt.event_id)
            {
            case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_INFO, semProvRouteLock, TAG, std::string(__func__) + "(): PROTOCOMM_SECURITY_SESSION_EVENT:PROTOCOMM_SECURITY_SESSION_SETUP_OK");
                break;
            }

            case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, std::string(__func__) + "(): PROTOCOMM_SECURITY_SESSION_EVENT:PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS");
                break;
            }

            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            {
                if (show & _showEvents)
                    logByValue(ESP_LOG_ERROR, semProvRouteLock, TAG, std::string(__func__) + "(): PROTOCOMM_SECURITY_SESSION_EVENT:PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH");
                break;
            }

            default:
            {
                logByValue(ESP_LOG_WARN, semProvRouteLock, TAG, std::string(__func__) + "(): PROTOCOMM_TRANSPORT_BLE_EVENT:default event_id is " + std::to_string(evt.event_id));
                break;
            }
            }
        }
    }

    return;
}