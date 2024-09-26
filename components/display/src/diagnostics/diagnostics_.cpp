#include "diagnostics/diagnostics_.hpp"
// #include "system_.hpp" // Class structure and variables

/* Debugging */
void Diagnostics::printTaskInfoByColumns(TaskHandle_t taskHandle) // This function is called when the System wants to compile an entire table of task information.
{
    char *name = pcTaskGetName(taskHandle); // Note: The value of NULL can be used as a parameter if the statement is running on the task of your inquiry.
    uint32_t priority = uxTaskPriorityGet(taskHandle);
    uint32_t highWaterMark = uxTaskGetStackHighWaterMark(taskHandle);
    printf("  %-10s   %02ld           %ld\n", name, priority, highWaterMark);
}
