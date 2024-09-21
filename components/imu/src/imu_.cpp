#include "imu/imu_.hpp"

//
// External Varibles
//
SemaphoreHandle_t semIMUEntry = nullptr;

IMU::IMU()
{


}

IMU::~IMU()
{

    
}

QueueHandle_t &IMU::getCmdRequestQueue(void) // Other items in the system need access to the I2C Request Queue
{
    return xQueueIMUCmdRequests;
}
