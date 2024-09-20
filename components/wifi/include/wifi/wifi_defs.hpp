#pragma once
#include "wifi/wifi_enums.hpp" // Local definitions, structs, and enumerations

/* showWifi */
#define _showWifiDirectiveSteps 0x01 // LSB
#define _showWifiConnSteps 0x02
#define _showWifiDiscSteps 0x04
#define _showWifiProvSteps 0x08 // NOTE: In a larger project I have universal provisioning working
#define _showWifiShdnSteps 0x10
#define _showWifiUnused1Steps 0x20
#define _showWifiUnused2Steps 0x40

/* wifiDirective */
#define _wifiClearPriHostInfo 0x01  // LSB
#define _wifiClearSecdHostInfo 0x02 // NOTE: Secondary Host is not support in this project
#define _wifiDisconnectHost 0x04    //
#define _wifiProvisionPriHost 0x08  //
#define _wifiProvisionSecdHost 0x10 // NOTE: Secondary Host is not support in this project
#define _wifiConnectPriHost 0x20    //
#define _wifiConnectSecdHost 0x40   // NOTE: Secondary Host is not support in this project
#define _wifiExitRun 0x80           // Not implemented

/* hostStatus */
#define _hostPriValid 0x01 // LSB
#define _hostPriActive 0x02
#define _hostPriUnused2 0x04
#define _hostPriUnused3 0x08
#define _hostSecdValid 0x10 // Secondary host is currently included inside this project
#define _hostSecdActive 0x20
#define _hostSecdUnused2 0x40
#define _hostSecdUnused3 0x80