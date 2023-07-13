//==========================================================================
//==========================================================================
#ifndef MAIN_SRC_SENSORS_H_
#define MAIN_SRC_SENSORS_H_

#include "esp_log.h"
#include "esp_err.h"

esp_err_t BatteryStatusUpdate();
void GPS_MeasureStart();
uint16_t GPS_SendData();
void Idle_Detection(void *args);

#endif /* MAIN_SRC_SENSORS_H_ */
