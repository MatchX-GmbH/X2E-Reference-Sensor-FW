//==========================================================================
//==========================================================================
#ifndef MAIN_SRC_SENSORS_H_
#define MAIN_SRC_SENSORS_H_

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//==========================================================================
// Variables
//==========================================================================
TaskHandle_t gSensorTaskHandle;

esp_err_t BatteryStatusUpdate();
uint32_t DataWaiting();
uint16_t GPS_SendData();
esp_err_t AccelerometerInit();
esp_err_t OdometerReset();
esp_err_t GPS_Init();
esp_err_t SensorsInit();
void SensorsTask(void *args);
void SensorPrepareForSleep(void);
void SensorResumeFromSleep(void);
bool SensorSleepReady(void);

#endif /* MAIN_SRC_SENSORS_H_ */
