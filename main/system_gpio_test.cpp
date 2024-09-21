#include "system_.hpp"

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"

#include "esp_pm.h"
#include "driver/rtc_io.h"
// #include "esp_clk.h"
#include "esp_clk_tree.h"
#include "esp_private/esp_clk.h"

#include "esp_sleep.h"

/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semWifiEntry;

//
// This source file is all about running tests.  All functions here are called only from our runGPIOTask() function.
//
// These will be grouped in these categories:
// 1) Object Lifecycle
// 2) Sleep Modes
// 3) NVS
// 4) Indication
// 5) WIFI
// 6)

//
// Object Lifecycle
//
void System::test_objectLifecycle_create(SYS_TEST_TYPE *type, uint8_t *index)
{
    switch (*index)
    {
    case 0:
    {
        if (wifi == nullptr)
            wifi = new Wifi();

        if (wifi != nullptr) // Make sure memory was allocated
        {
            if (xSemaphoreTake(semWifiEntry, 100)) // Get a lock on the object after it initializes
            {
                taskHandleWifiRun = wifi->getRunTaskHandle();
                queHandleWIFICmdRequest = wifi->getCmdRequestQueue();
                xSemaphoreGive(semWifiEntry); // Release lock

                // Send out notifications to any object that needs the wifi and tell them wifi is now available.

                ESP_LOGW(TAG, "wifi instantiated");
            }
        }
        *type = SYS_TEST_TYPE::LIFE_CYCLE_DESTROY;
        *index = 0;
        break;
    }

    case 1:
    {
        break;
    }

    case 2:
    {
        break;
    }
    }
}

void System::test_objectLifecycle_destroy(SYS_TEST_TYPE *type, uint8_t *index)
{
    switch (*index)
    {
    case 0:
    {
        if (wifi != nullptr)
        {
            if (semWifiEntry != nullptr)
            {
                xSemaphoreTake(semWifiEntry, portMAX_DELAY); // Wait here until we gain the lock.

                // Send out notifications to any object that uses the wifi and tell them wifi is no longer available.

                taskHandleWIFIRun = nullptr;       // Reset the wifi handles
                queHandleWIFICmdRequest = nullptr; //

                delete wifi;                   // Lock on the object will be done inside the destructor.
                wifi = nullptr;                // Destructor will not set pointer null.  We have to do that manually.
                ESP_LOGW(TAG, "wifi deleted"); //

                // Note: The semWifiEntry semaphore is already destroyed - so don't "Give" it or a run time error will occur
            }
        }
        *type = SYS_TEST_TYPE::LIFE_CYCLE_CREATE;
        *index = 0;
        return;
        break;
    }

    case 1:
    {
        break;
    }

    case 2:
    {
        break;
    }
    }

    if (++*index > 0) // We set the limit based on our test sequence
    {
        ESP_LOGW("", "Restarting text index...");
        *index = 0;
    }
}

//
// Low Power and Sleep Modes
//
void System::test_power_management(SYS_TEST_TYPE *type, uint8_t *index)
{
    switch (*index)
    {
    case 0:
    {
        // Use of Power Management creates interrupt latency because the system required time to resumes power consuming activities.
        // If you always need a minimum response time then you should not use Power Management features.
        // Note to self: APB is the Advanced Peripheral Bus (for those of your like me who can't remember every anacronym)

        // All of the following menuConfig commands should be in place to take advantage of Power Management.  Further, I generally suggest
        // that you remove them if you are not taking advantage of Power Management services.

        // (Order of item selection is important for this list...)
        //
        // # Enable support for power management
        // CONFIG_PM_ENABLE=y        // [X] Support for power management
        // CONFIG_PM_DFS_INIT_AUTO=y // [X] Enable dynamic frequency scaling (DFS) at startup
        // CONFIG_PM_PROFILING=y     // [X] Enable profiling counters for PM locks

        // # Kernel
        // CONFIG_FREERTOS_USE_TICKLESS_IDLE=y // [X] configUSE_TICKLESS_IDLE

        // # Put related source code in IRAM
        // CONFIG_PM_SLP_IRAM_OPT=y  // [X] Put lightsleep related codes in internal RAM
        // CONFIG_PM_RTOS_IDLE_OPT=y // [X] Put RTOS IDLE related codes in internal RAM

        // # Enable wifi sleep iram optimization
        // CONFIG_ESP_WIFI_SLP_IRAM_OPT=y // [X] WiFi SLP IRAM speed optimization

        ESP_LOGW(TAG, "Activating Power Management...");
        ESP_ERROR_CHECK(uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM));

        // This hardware has an active GPIO0 input.  We must register this input as a wake up source AND allow the VddSDIO power bus to remain on.
        // If we don't run these next 2 or 3 lines of code, the GPIO0 line will bounce and trigger unintended interrupts.
        // We mainly need these commands in place for pm_config.light_sleep_enable = true AND we have a timer or some other interrupt source triggering sleep-wake cycles.
        ESP_ERROR_CHECK(gpio_wakeup_enable(SW1, GPIO_INTR_LOW_LEVEL)); // Our GPIO0 is already configured for input active LOW
        ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON)); // This feature may already be in AUTO mode before you arrive here and this statement may be superfluous.

        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,        // These values are set manually here.
            .min_freq_mhz = 80,         //
            .light_sleep_enable = true, // Automatic light sleep can be enabled if tickless idle support is enabled.
        };

        ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "cpu clock frequency is %ld", freqValue);

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "apb clock frequency is %ld", freqValue);

        esp_pm_dump_locks(stdout);

        *type = SYS_TEST_TYPE::IDLE; // Idle process allows us to dump PM info on the next GPIO0 input.
        // *index = 1;
        break;
    }

    case 1:
    {
        ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_lock", &cpu_lock_handle));
        ESP_ERROR_CHECK(esp_pm_lock_acquire(cpu_lock_handle));
        taskYIELD();

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "cpu clock frequency aquired is %ld", freqValue);

        ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "apb_lock", &apb_lock_handle));
        ESP_ERROR_CHECK(esp_pm_lock_acquire(apb_lock_handle));
        taskYIELD();

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "apb clock frequency aquired is %ld", freqValue);

        esp_pm_dump_locks(stdout);

        *index = 2;
        break;
    }

    case 2:
    {
        ESP_ERROR_CHECK(esp_pm_lock_release(cpu_lock_handle));
        ESP_ERROR_CHECK(esp_pm_lock_delete(cpu_lock_handle));
        cpu_lock_handle = nullptr;
        taskYIELD();

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "cpu clock frequency released is %ld", freqValue);

        ESP_ERROR_CHECK(esp_pm_lock_release(apb_lock_handle));
        ESP_ERROR_CHECK(esp_pm_lock_delete(apb_lock_handle));
        apb_lock_handle = nullptr;
        taskYIELD();

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "apb clock frequency released is %ld", freqValue);

        esp_pm_dump_locks(stdout);

        //*type = SYS_TEST_TYPE::WIFI;
        *index = 0;
        break;
    }
    }
}

void System::test_light_sleep(SYS_TEST_TYPE *type, uint8_t *index)
{
    switch (*index)
    {
    case 0: // Enter Light Sleep (Full RAM Retention)
    {
        while (gpio_get_level(SW1) == 0)   // This is required to allow time for the circuit to rise after the interrupt trigger (low).
            vTaskDelay(pdMS_TO_TICKS(10)); // which brought the call here.  We need to see a high before we can continue.  This takes time.

        ESP_ERROR_CHECK(gpio_wakeup_enable(SW1, GPIO_INTR_LOW_LEVEL)); // Our GPIO0 is already configured for input active LOW
        ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "cpu clock frequency final read is %ld", freqValue);

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "apb clock frequency final read is %ld", freqValue);

        // WARNING: You will need to be sure that you are attached to the correct Console UART or you may not see the serial output
        //          resume when you awake from sleep.  Select the serial port output that is native to the Tx/Rx pins - not the on-chip USB output.

        // esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO); // I did this for a test to confirm that GPIO can be disabled as the wakeup source.

        ESP_LOGW(TAG, "Entering Light Sleep...");
        ESP_ERROR_CHECK(uart_wait_tx_idle_polling((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM)); // This call flushes the serial port FIFO buffer before sleep

        // By default, esp_deep_sleep_start() and esp_light_sleep_start() functions will power down all RTC power domains which are not
        // needed by the enabled wakeup sources.  To override this behaviour, esp_sleep_pd_config() function is provided.
        // All examples here are the default where they are powered off if not needed (AUTO).
        // You may over-ride with the option ESP_PD_OPTION_ON where that domains remains on during sleep.
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_AUTO));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_AUTO));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_AUTO));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_AUTO));
        ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_AUTO));

        esp_light_sleep_start();
        //
        // The active interrupt on this GPIO0 pin doesn't seem to have an effect on our wake-up input
        // which occurs right here...  I'm guessing that this interrupt doesn't get registered in light sleep
        // or it is lost/ignored in the wake-up process.
        //
        ESP_LOGW(TAG, "Returning from Light Sleep...");
        ESP_ERROR_CHECK(gpio_wakeup_disable(SW1));

        esp_pm_dump_locks(stdout);

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "cpu clock frequency final read is %ld", freqValue);

        ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_APB, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &freqValue));
        ESP_LOGW(TAG, "apb clock frequency final read is %ld", freqValue);

        // *type = SYS_TEST_TYPE::WIFI;
        *index = 0;
        break;
    }
    }

    // CHANGE YOUR TEST AREA AND INDEX AS NEEDED FOR THE NEXT SEQUENCE YOU WANT
}

void System::test_deep_sleep(SYS_TEST_TYPE *type, uint8_t *index)
{
    // Deep sleep will by default shut down CPU_CLK, APB_CLK, and RAM.

    switch (*index)
    {
    case 0: // Enter Deep Sleep
    {
        while (gpio_get_level(SW1) == 0)   // This is required to allow time for the circuit to rise after the interrupt trigger (low).
            vTaskDelay(pdMS_TO_TICKS(10)); // which brought the call here.  We need to see a high before we can continue.  This takes time.

        // Isolate RTC pins that are active and might pull current during sleep. RTC pins available for the ESP32-S3:0-21.
        // rtc_gpio_isolate(GPIO_NUM_1);
        // rtc_gpio_isolate(GPIO_NUM_2);

        // Disable any interrupt source which should not wake the system.
        // (None at this time)

        // Enable required wake up sources.  RTC pins available for the ESP32-S3: 0-21.
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(SW1, 0));

        // Enable pull-up/pull-down on the required input pins
        ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(SW1)); // Always disable a source or a sink first
        ESP_ERROR_CHECK(rtc_gpio_pullup_en(SW1));    // Enable a source or a sink second

        ESP_LOGW(TAG, "Entering Deep Sleep..."); // Don't need to flush the serial port before sleep -- normal operation will do this for you with deep sleep.
        esp_deep_sleep_start();
        // There is no returning to this point.  Every deep-sleep wake up will restart the application at app_main()
        break;
    }
    }
}

//
// NVS
//
void System::test_nvs(SYS_TEST_TYPE *type, uint8_t *index)
{
    // bool testBool = false;
    // std::string testString = "";
    // std::string string1 = "The quick brown fox jumps over the lazy dog";
    // std::string string2 = "Now is the time for all good men to come to the aid of their country.";
    // uint8_t testUInteger8 = 0;
    // uint32_t testUInteger32 = 0;
    // int32_t testInteger32 = 555; // Should test number going negative

    switch (*index)
    {
    case 0:
    {
        if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        {
            ESP_LOGW(TAG, "eraseNVSPartition");
            nvs->eraseNVSPartition();
            esp_restart(); // Must do a complete reboot immediately or system will crash in any nvs instruction.  Must completely reinitialize nvs at startup.
            xSemaphoreGive(semNVSEntry);
        }
        ++*index;
        break;
    }

    case 1:
    {
        break;
    }

    case 2:
    {
        break;
    }
    }

    // CHANGE YOUR TEST AREA AND INDEX AS NEEDED FOR THE NEXT SEQUENCE YOU WANT
}

//
// Wifi
//
void System::test_wifi(SYS_TEST_TYPE *type, uint8_t *index)
{
    switch (*index)
    {
    case 0:
    {
        while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_CLEAR_PRI_HOST), eSetValueWithoutOverwrite))
             vTaskDelay(pdMS_TO_TICKS(50));

        while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_PROV_HOST), eSetValueWithoutOverwrite))
            vTaskDelay(pdMS_TO_TICKS(50));

        while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_RUN_DIRECTIVES), eSetValueWithoutOverwrite))
            vTaskDelay(pdMS_TO_TICKS(50));
        ++*index;
        break;
    }

    case 1:
    {
        printTaskInfo();
        ++*index;
        break;
    }

    case 2:
    {
        // while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_DISC_HOST), eSetValueWithoutOverwrite))
        //     vTaskDelay(pdMS_TO_TICKS(50));

        while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_CONN_PRI_HOST), eSetValueWithoutOverwrite))
            vTaskDelay(pdMS_TO_TICKS(50));

        while (!xTaskNotify(taskHandleWIFIRun, static_cast<uint32_t>(WIFI_NOTIFY::CMD_RUN_DIRECTIVES), eSetValueWithoutOverwrite))
            vTaskDelay(pdMS_TO_TICKS(50));
        ++*index;
        break;
    }

    case 3:
    {
        printTaskInfo();
        *index = 0;
        break;
    }
    }

    // CHANGE YOUR TEST AREA AND INDEX AS NEEDED FOR THE NEXT SEQUENCE YOU WANT
}