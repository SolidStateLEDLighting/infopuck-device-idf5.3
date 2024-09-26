#pragma once
#include "prov/prov_defs.hpp" // Local definitions, structs, and enumerations
#include "system_.hpp"        // Class structure and variables

#include <string> // Standard libraries

#include <wifi_provisioning/manager.h> // ESP libraries
#include <wifi_provisioning/scheme_softap.h>

#include "logging/logging_.hpp"
#include "diagnostics/diagnostics_.hpp"

ESP_EVENT_DECLARE_BASE(PROTOCOMM_SECURITY_SESSION_EVENT);

/* Forward Declarations */
class System;
class NVS;

extern "C"
{
    class PROV : private Logging, private Diagnostics // The "IS-A" relationship
    {
    public:
        PROV(QueueHandle_t);
        ~PROV();

        TaskHandle_t taskHandleRun = nullptr; // We reach into this object to copy the task handle. (NOT task safe)

    private:
        PROV(const PROV &) = delete;           // Disable copy constructor
        void operator=(PROV const &) = delete; // Disable assignment operator

        char TAG[6] = "_prov";

        System *sys = nullptr; // Object References
        NVS *nvs = nullptr;    //

        uint8_t show = 0;
        uint8_t showPROV = 0;

        QueueHandle_t wifiQueCmdRequests;
        QueueHandle_t queueEvents = nullptr;

        void setFlags(void);
        void setLogLevels(void);
        void setConditionalCompVariables(void);
        void createSemaphores(void);
        void createQueues(void);
        void destroySemaphores(void);
        void destroyQueues(void);

        /* Provisioning variables */
        esp_netif_t *defaultSTANetif = nullptr; // Default STA and SAP handles
        esp_netif_t *defaultSAPNetif = nullptr;

        int provFailureRetries = 0;

        esp_event_handler_instance_t instanceHndProvEvents = nullptr; // Event Registration handles
        esp_event_handler_instance_t instanceHndSessionEvents = nullptr;

        wifi_prov_security_t security;
        wifi_prov_security1_params_t *sec_params = nullptr;
        wifi_prov_security2_params_t sec2_params = {};

        uint8_t postProcessWaitCount = 0;

        std::string ssidUnderTest = "empty";    // Held in Wifi but only accessed in an eventHandler (no locking used)
        std::string ssidPwdUnderTest = "empty"; // Held in Wifi but only accessed in an eventHandler (no locking used)

        std::string userName = "wifiprov"; // This is a default development mode userName
        std::string pop = "abcd1234";      // This is a default development mode pop
        char *ptrPOP = nullptr;

        bool blnCancelProvisioning = false;

        bool blnAllowReprovisioning = false;
        bool blnResetProvisioning = false;
        bool blnSecurityVersion1 = false;
        bool blnSecurityVersion2 = false;
        bool blnResetProvMgrOnFailure = false;
        uint8_t maxFailureRetryCount = 3;
        bool blnSec2DevelopmentMode = false;
        bool blnSec2ProductionMode = false;

        wifi_prov_mgr_config_t config;

        /* PROV_Events */
        static void eventHandlerProvisionMarshaller(void *, esp_event_base_t, int32_t, void *);
        void eventHandlerProvision(esp_event_base_t, int32_t, void *);
        static esp_err_t eventHandlerCustomMarshaller(uint32_t, const uint8_t *, ssize_t, uint8_t **, ssize_t *, void *);
        esp_err_t eventHandlerCustom(uint32_t, const uint8_t *, ssize_t, uint8_t **, ssize_t *);

        /* PROV_Run */
        PROV_OP provOP = PROV_OP::Idle;        // Object States
        PROV_RUN provRunStep = PROV_RUN::Idle; //

        static void runMarshaller(void *); // Run functions
        void run(void);                    //
        void runEvents(void);              //

        /* PROV_Utilities */
        void getDeviceServiceName(char *, size_t);

        esp_err_t example_get_sec2_salt(const char **, uint16_t *);
        esp_err_t example_get_sec2_verifier(const char **, uint16_t *);
    };
}