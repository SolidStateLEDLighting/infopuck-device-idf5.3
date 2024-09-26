#include "i2c/i2c_.hpp"
#include "system_.hpp"

/* External Semaphores */
extern SemaphoreHandle_t semI2CEntry;
extern SemaphoreHandle_t semI2CRouteLock;

void I2C::runMarshaller(void *arg)
{
    ((I2C *)arg)->run();
    vTaskDelete(NULL);
    ((I2C *)arg)->taskHandleRun = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it to nullptr manually.
}

void I2C::run(void) // I2C processing lives here for the lifetime of the object
{
    esp_err_t ret = ESP_OK;

    while (true) // Process all incomeing I2C requests and return any results
    {
        switch (i2cOP)
        {
        case I2C_OP::Run:
        {
            if (xQueueReceive(queueCmdRequests, &ptrI2CCmdRequest, pdMS_TO_TICKS(1500))) // We wait here for each message
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

            switch (ptrI2CCmdRequest->command)
            {
            case I2C_COMMAND::Read_Bytes_RegAddr: // Length must be greater than zero
            {
                if (ptrI2CCmdRequest->dataLength < 1) // Must always have at least one addressed byte to read
                {
                    ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
                    ptrI2CCmdResponse->dataLength = 0;
                }
                else
                {
                    readBytesRegAddr(ptrI2CCmdRequest->busDevAddress, ptrI2CCmdRequest->deviceRegister, ptrI2CCmdRequest->dataLength, ptrI2CCmdResponse->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdRequest->QueueToSendResponse, &ptrI2CCmdResponse, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_COMMAND::Write_Bytes_RegAddr:
            {
                if (ptrI2CCmdRequest->dataLength < 1) // Must always have at least one addressed byte to read
                {
                    ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
                    ptrI2CCmdResponse->dataLength = 0;
                }
                else
                {
                    writeBytesRegAddr(ptrI2CCmdRequest->busDevAddress, ptrI2CCmdRequest->deviceRegister, ptrI2CCmdRequest->dataLength, ptrI2CCmdRequest->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdRequest->QueueToSendResponse, &ptrI2CCmdResponse, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_COMMAND::Read_Bytes_Immediate:
            {
                // We are reading some number of immediate values (no target register is selected).   This technique is occasionally
                // used in devices which need rapid access and the time it takes to select a target register first is undesirable.
                //
                if (ptrI2CCmdRequest->dataLength < 1) // Must always ask for at least one non-addressed byte to read
                {
                    ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
                    ptrI2CCmdResponse->dataLength = 0;
                }
                else
                {
                    readBytesImmediate(ptrI2CCmdRequest->busDevAddress, ptrI2CCmdRequest->dataLength, ptrI2CCmdRequest->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdRequest->QueueToSendResponse, &ptrI2CCmdResponse, 50);
                i2cOP = I2C_OP::Run;
                break;
            }

            case I2C_COMMAND::Write_Bytes_Immediate:
            {
                if (ptrI2CCmdRequest->dataLength < 1) // Must always ask for at least one non-addressed byte to read
                {
                    ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
                    ptrI2CCmdResponse->dataLength = 0;
                }
                else
                {
                    writeBytesImmediate(ptrI2CCmdRequest->busDevAddress, ptrI2CCmdRequest->dataLength, ptrI2CCmdRequest->data, defaultTimeout);
                }
                xQueueSendToBack(ptrI2CCmdRequest->QueueToSendResponse, &ptrI2CCmdResponse, 50);
                i2cOP = I2C_OP::Run;
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

                ESP_LOGI(TAG, "portNum=%d, sda=%d, scl=%d, clockSpeed=%ld  timeout=%ld", I2C_PORT_0, SDA_PIN_0, SCL_PIN_0, defaultClockSpeed, defaultTimeout);
                assert(I2C_PORT_0 < I2C_NUM_MAX);
                [[fallthrough]];
            }

            case I2C_INIT::Load_NVS_Settings: // No settings to load at this time.
            {
                if (showInitSteps)
                    ESP_LOGI(TAG, "Step 1  - Load_NVS_Settings");
                initI2CStep = I2C_INIT::Create_Master_Bus;
                [[fallthrough]];
                // In other larger systems, we might needed to initialize an I2C Mux.  That would be done here.
            }

            case I2C_INIT::Create_Master_Bus:
            {
                i2c_master_bus_config_t i2c_mst_config;
                i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
                i2c_mst_config.i2c_port = I2C_PORT_0;
                i2c_mst_config.scl_io_num = SCL_PIN_0;
                i2c_mst_config.sda_io_num = SDA_PIN_0;
                i2c_mst_config.glitch_ignore_cnt = 7;
                i2c_mst_config.flags.enable_internal_pullup = true;

                ESP_GOTO_ON_ERROR(i2c_new_master_bus(&i2c_mst_config, &bus_handle), i2c_I2C_run_err, TAG, "i2c_new_master_bus() failed");
                initI2CStep = I2C_INIT::Scan;
                break;
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

i2c_I2C_run_err:
    logByValue(ESP_LOG_ERROR, semI2CRouteLock, TAG, std::string(__func__) + "(): error: " + esp_err_to_name(ret));
}

void I2C::readBytesRegAddr(uint8_t devAddr, uint8_t regAddr, size_t length, uint8_t *data, int32_t timeout)
{
    //
    // You'll notice that we do an i2c restart in the middle of our cycle here once we have written the target register address.
    //
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK_EN);
    i2c_master_write_byte(cmd, regAddr, I2C_MASTER_ACK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, I2C_MASTER_ACK_EN);
    i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t rc = i2c_master_cmd_begin(i2c_port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    ptrI2CCmdResponse->deviceRegister = regAddr;
    ptrI2CCmdResponse->dataLength = length;

    if (rc == ESP_OK)
    {
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Data;
    }
    else
    {
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
        ptrI2CCmdResponse->dataLength = 1;
        ptrI2CCmdResponse->data[0] = rc;
    }

    if (ptrI2CCmdRequest->debug)
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
    if (ptrI2CCmdRequest->debug)
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
    esp_err_t rc = i2c_master_cmd_begin(i2c_port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Ack;
    else
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;

    ptrI2CCmdResponse->deviceRegister = ptrI2CCmdRequest->deviceRegister;
    ptrI2CCmdResponse->dataLength = 1;
    ptrI2CCmdResponse->data[0] = rc;
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
    esp_err_t rc = i2c_master_cmd_begin(i2c_port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    ptrI2CCmdResponse->dataLength = length;

    if (rc == ESP_OK)
    {
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Data;
    }
    else
    {
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;
        ptrI2CCmdResponse->dataLength = 1;
        ptrI2CCmdResponse->data[0] = rc;
    }

    if (ptrI2CCmdRequest->debug)
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
    if (ptrI2CCmdRequest->debug)
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
    esp_err_t rc = i2c_master_cmd_begin(i2c_port, cmd, (timeout < 0 ? ticksToWait : pdMS_TO_TICKS(timeout)));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Ack;
    else
        ptrI2CCmdResponse->response = I2C_RESPONSE::Returning_Error;

    ptrI2CCmdResponse->deviceRegister = ptrI2CCmdRequest->deviceRegister;
    ptrI2CCmdResponse->dataLength = 1;
    ptrI2CCmdResponse->data[0] = rc;
}
//
// Utility Functions
//
void I2C::busScan() // Scan the I2C bus looking for devices.
{
    // A scan is performed on the I2C bus looking for devices.  A table is written
    // to the serial output describing what devices(if any) were found.
    //
    constexpr int32_t scanTimeoutMS = 100;
    uint8_t AddressCount = 0;

    printf("...................................................\n");
    printf("  Data Pin: %d, Clock Pin: %d, Port Speed: %ld\n", SDA_PIN_0, SCL_PIN_0, defaultClockSpeed);
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
    auto rc = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(timeoutMS));
    i2c_cmd_link_delete(cmd);

    if (rc == ESP_OK)
        return true;
    else
        return false;
}
