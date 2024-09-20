#include "system_.hpp"
#include "esp_ota_ops.h"

#include "esp_system.h"

extern "C" void app_main(void)
{
    // Upon startup, there is always a reset reason.  With a cold boot or the reset button, the startup reason is ESP_RST_POWERON.
    // When we are waking up fron Deep Sleep, the startup reason is ESP_RST_DEEPSLEEP.  We pass in that value so
    // the System to wake up in the correct way according to the startup reason.
    __attribute__((unused)) auto sys = System::getInstance(esp_reset_reason()); // Creating the system singleton object...
}
