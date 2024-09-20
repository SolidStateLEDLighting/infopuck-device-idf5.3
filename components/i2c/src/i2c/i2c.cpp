#include "i2c/i2c_.hpp"

SemaphoreHandle_t semI2CEntry = nullptr;

I2C::I2C(i2c_port_t myPort, gpio_num_t mySda, gpio_num_t myScl, uint32_t mySpeed, uint32_t myTimeout)
{
    port = myPort;
    i2c_sda_pin = mySda;
    i2c_scl_pin = myScl;
    clock_speed = mySpeed;
    defaultTimeout = myTimeout;

    //
    // Set Object Debug Level
    // Note that this function can not raise log level above the level set using CONFIG_LOG_DEFAULT_LEVEL setting in menuconfig.
    esp_log_level_set(TAG, ESP_LOG_INFO);

    assert(port < I2C_NUM_MAX);

    if (showInitSteps)
        ESP_LOGI(TAG, "portNum=%d, sda=%d, scl=%d, clockSpeed=%ld  timeout=%ld", port, i2c_sda_pin, i2c_scl_pin, clock_speed, defaultTimeout);
    //
    // Get the port open first, then establish all the other support objects/structures
    //
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = i2c_sda_pin;
    conf.scl_io_num = i2c_scl_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = clock_speed;
    conf.clk_flags = 0;

    esp_err_t errRc = i2c_param_config(port, &conf);
    if (errRc != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_param_config: rc=%d 0x%X", errRc, errRc);
        return;
    }

    errRc = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (errRc != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_driver_install: rc=%d 0x%X", errRc, errRc);
        return;
    }

    if (showInitSteps)
        ESP_LOGI(TAG, "I2C Port %d has been initialized...", port);
    //
    // Establish support items
    //
    vSemaphoreCreateBinary(semI2CEntry);
    if (semI2CEntry != nullptr)
    {
        xSemaphoreTake(semI2CEntry, portMAX_DELAY); // We hold our entry semaphore until full initialization is complete
    }
    //
    // Message Queues
    //
    xQueueI2CCmdRequests = xQueueCreate(1, sizeof(I2C_CmdRequest *));
    ptrI2CCmdReq = new I2C_CmdRequest();
    ptrI2CCmdResp = new I2C_Response();
    //
    // Start our task and state machine
    //
    i2cOP = I2C_OP::Init;
    initI2CStep = I2C_INIT::Start;
    xTaskCreate(runMarshaller, "I2C::Run", 1024 * 3, this, 8, &taskHandleI2C);
}

I2C::~I2C()
{
    TaskHandle_t temp = this->taskHandleI2C;
    this->taskHandleI2C = nullptr;
    vTaskDelete(temp);

    i2c_driver_delete(port);

    if (xQueueI2CCmdRequests != nullptr)
        vQueueDelete(xQueueI2CCmdRequests);

    if (ptrI2CCmdReq != NULL)
        delete (ptrI2CCmdReq);

    if (ptrI2CCmdResp != NULL)
        delete (ptrI2CCmdResp);

    if (semI2CEntry != NULL)
        vSemaphoreDelete(semI2CEntry);
}

QueueHandle_t &I2C::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return xQueueI2CCmdRequests;
}

void I2C::runMarshaller(void *arg)
{
    auto obj = (I2C *)arg;
    obj->run();

    if (obj->taskHandleI2C == nullptr)
        return;

    auto temp = obj->taskHandleI2C;
    obj->taskHandleI2C = nullptr;
    vTaskDelete(temp);
}

void I2C::run(void) // I2C processing lives here for the lifetime of the object
{
    while (true) // Process all incomeing I2C requests and return any results
    {
        switch (i2cOP)
        {
        case I2C_OP::Run:
        {
            if (xQueueReceive(xQueueI2CCmdRequests, &ptrI2CCmdReq, pdMS_TO_TICKS(1500))) // We wait here for each message
                i2cOP = I2C_OP::ReadWriteI2CBus;

            // By default, this is the correct processing path...

            if (showRun)
                ESP_LOGI(TAG, "I2C run");
            break;
        }
            // The primary approach of our work here are to establish and execute complete transactions.  All the data for the transaction
            // must arrive at once.   The resultant data must all be storable at once.   Currently, this restricts data to the size of the
            // allocated storage buffers in the I2C_CmdRequest/I2C_Response structures.
        case I2C_OP::ReadWriteI2CBus:
        {
            // ESP_LOGI(TAG, "     RECEIVED MESSAGE");
            // ESP_LOGI(TAG, "     TargetAddress 0x%02X", ptrI2CCmdReq->busDevAddress);
            // ESP_LOGI(TAG, "     Action %d", (uint8_t)ptrI2CCmdReq->action);
            // ESP_LOGI(TAG, "     RegisterAddress 0x%02X", ptrI2CCmdReq->deviceRegister);
            // ESP_LOGI(TAG, "     DataLength %d", ptrI2CCmdReq->dataLength);
            // ESP_LOGI(TAG, "     Data 0x%02X", ptrI2CCmdReq->data[0]);
            // i2cOP = I2C_OP::Run;
            // break;

            switch (ptrI2CCmdReq->action)
            {
            case I2C_ACTION::Read_Bytes_RegAddr: // Length must be greater than zero
            {
                if (ptrI2CCmdReq->dataLength < 1) // Must always have at least one addressed byte to read
                {
                    ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
                    ptrI2CCmdResp->dataLength = 0;
                }
                else
                {
                    readBytesRegAddr(ptrI2CCmdReq->busDevAddress, ptrI2CCmdReq->deviceRegister, ptrI2CCmdReq->dataLength, ptrI2CCmdResp->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdReq->QueueToSendResponse, &ptrI2CCmdResp, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_ACTION::Write_Bytes_RegAddr:
            {
                if (ptrI2CCmdReq->dataLength < 1) // Must always have at least one addressed byte to read
                {
                    ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
                    ptrI2CCmdResp->dataLength = 0;
                }
                else
                {
                    writeBytesRegAddr(ptrI2CCmdReq->busDevAddress, ptrI2CCmdReq->deviceRegister, ptrI2CCmdReq->dataLength, ptrI2CCmdReq->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdReq->QueueToSendResponse, &ptrI2CCmdResp, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_ACTION::Read_Bytes_Immediate:
            {
                // We are reading some number of immediate values (no target register is selected).   This technique is occasionally
                // used in devices which need rapid access and the time it takes to select a target register first is undesirable.

                if (ptrI2CCmdReq->dataLength < 1) // Must always ask for at least one non-addressed byte to read
                {
                    ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
                    ptrI2CCmdResp->dataLength = 0;
                }
                else
                {
                    readBytesImmediate(ptrI2CCmdReq->busDevAddress, ptrI2CCmdReq->dataLength, ptrI2CCmdResp->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdReq->QueueToSendResponse, &ptrI2CCmdResp, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_ACTION::Write_Bytes_Immediate:
            {
                if (ptrI2CCmdReq->dataLength < 1) // Must always ask for at least one non-addressed byte to read
                {
                    ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
                    ptrI2CCmdResp->dataLength = 0;
                }
                else
                {
                    writeBytesImmediate(ptrI2CCmdReq->busDevAddress, ptrI2CCmdReq->dataLength, ptrI2CCmdReq->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdReq->QueueToSendResponse, &ptrI2CCmdResp, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_ACTION::Returning_Data:
            {
                break;
            }

            case I2C_ACTION::Returning_Ack:
            {
                break;
            }

            case I2C_ACTION::Returning_Nak:
            {
                break;
            }

            case I2C_ACTION::Returning_Error:
            {
                break;
            }
            }
            break;
        }

        case I2C_OP::Init: // Go through any slow initialization requirements here...
        {
            switch (initI2CStep)
            {
            case I2C_INIT::Start:
            {
                ESP_LOGI(TAG, "Initalization Start");
                initI2CStep = I2C_INIT::Load_NVS_Settings;
                [[fallthrough]];
            }

            case I2C_INIT::Load_NVS_Settings: // No settings to load at this time.
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 1  - Load_NVS_Settings");
                initI2CStep = I2C_INIT::Scan;
                [[fallthrough]];
                // In other larger systems, we might needed to initialize an I2C Mux.  That would be done here.
            }

            case I2C_INIT::Scan:
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 2  - Scan");
                busScan();
                initI2CStep = I2C_INIT::Finished;
                break;
            }

            case I2C_INIT::Finished:
            {
                ESP_LOGI(TAG, "Initialization Finished");
                i2cOP = I2C_OP::Run;
                xSemaphoreGive(semI2CEntry);
                break;
            }
            }
            break;
        }
        }
    }
}
//
// You'll notice that we do a restart in the middle of our cycle here once we have written the target register address.
//
void I2C::readBytesRegAddr(uint8_t devAddr, uint8_t regAddr, size_t length, uint8_t *data, int32_t timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
    i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, I2C_MASTER_ACK_EN);
    i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    ptrI2CCmdResp->deviceRegister = regAddr;
    ptrI2CCmdResp->dataLength = length;

    if (rc == ESP_OK)
    {
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Data;
    }
    else
    {
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
        ptrI2CCmdResp->dataLength = 1;
        ptrI2CCmdResp->data[0] = rc;
    }

    if (ptrI2CCmdReq->debug)
    {
        ESP_LOGI(TAG, "          readBytesRegAddr devAddr %02X regAddr %02X length %d timeout %ld", devAddr, regAddr, length, timeout);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "          data[%d] 0x%02X", i, data[i]);
        }
    }
}

void I2C::writeBytesRegAddr(uint8_t devAddr, uint8_t regAddr, size_t length, uint8_t *data, int32_t timeout)
{
    if (ptrI2CCmdReq->debug)
    {
        ESP_LOGI(TAG, "          writeBytesRegAddr devAddr %02X regAddr %02X length %d timeout %ld", devAddr, regAddr, length, timeout);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "          data[%d] 0x%02X", i, data[i]);
        }
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
    i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);
    i2c_master_write(cmd, (uint8_t *)data, length, I2C_MASTER_ACK_EN);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Ack;
    else
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;

    ptrI2CCmdResp->deviceRegister = ptrI2CCmdReq->deviceRegister;
    ptrI2CCmdResp->dataLength = 1;
    ptrI2CCmdResp->data[0] = rc;
}
//
// We use the read Immediate calls when there is no register address to target prior to the read/write.  By default
// Immediate reads/writes typically start from address 0 and progress towards the end of the memory map sequentially.
// Exact implementations may vary.  Omitting a starting register address speeds up the process.
//
void I2C::readBytesImmediate(uint8_t devAddr, size_t length, uint8_t *data, int32_t timeout)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, I2C_MASTER_ACK_EN);
    i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    ptrI2CCmdResp->dataLength = length;

    if (rc == ESP_OK)
    {
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Data;
    }
    else
    {
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;
        ptrI2CCmdResp->dataLength = 1;
        ptrI2CCmdResp->data[0] = rc;
    }

    if (ptrI2CCmdReq->debug)
    {
        ESP_LOGI(TAG, "          readBytesImmediate devAddr %02X length %d timeout %ld", devAddr, length, timeout);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "          data[%d] 0x%02X", i, data[i]);
        }
    }
}

void I2C::writeBytesImmediate(uint8_t devAddr, size_t length, uint8_t *data, int32_t timeout)
{
    if (ptrI2CCmdReq->debug)
    {
        ESP_LOGI(TAG, "          writeBytesImmediate devAddr %02X length %d timeout %ld", devAddr, length, timeout);
        for (int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "          data[%d] 0x%02X", i, data[i]);
        }
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
    i2c_master_write(cmd, data, length, I2C_MASTER_ACK_EN);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Ack;
    else
        ptrI2CCmdResp->result = I2C_ACTION::Returning_Error;

    ptrI2CCmdResp->deviceRegister = ptrI2CCmdReq->deviceRegister;
    ptrI2CCmdResp->dataLength = 1;
    ptrI2CCmdResp->data[0] = rc;
}

void I2C::busScan() // Scan the I2C bus looking for devices.
{
    // A scan is performed on the I2C bus looking for devices.  A table is written
    // to the serial output describing what devices(if any) were found.
    //
    constexpr int32_t scanTimeoutMS = 100;
    uint8_t AddressCount = 0;

    printf("...................................................\n");
    printf("  Data Pin: %d, Clock Pin: %d, Port Speed: %ld\n", this->i2c_sda_pin, this->i2c_scl_pin, this->clock_speed);
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");
    for (auto i = 3; i < 0x78; i++)
    {
        if (i % 16 == 0)
            printf("\n%.2x:", i);

        if (slavePresent(i, scanTimeoutMS))
        {
            printf(" %.2X", i);
            AddressCount++;
        }
        else
            printf(" --");
    }
    // printf("\n");
    printf("   Found %d Active Devices\n", AddressCount);
    printf("...................................................\n");
}

bool I2C::slavePresent(uint8_t devAddress, int32_t timeoutMS) // Determine if the slave is present and responding.
{
    auto cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddress << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN); // expect ack
    i2c_master_stop(cmd);
    auto rc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(timeoutMS));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        return true;
    else
        return false;
}
