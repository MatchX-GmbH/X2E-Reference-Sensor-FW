//==========================================================================
//==========================================================================
#ifndef INC_SENSOR_H
#define INC_SENSOR_H

//==========================================================================
//==========================================================================
#include "mx_target.h"

//==========================================================================
//==========================================================================
int8_t SensorInit(void);
void SensorPrepareForSleep(void);
void SensorResumeFromSleep(void);

void SensorGetBattery(float *aPrecentage, float *aCurrent, float *aVoltage);

//==========================================================================
//==========================================================================
#endif // INC_SENSOR_OP_H