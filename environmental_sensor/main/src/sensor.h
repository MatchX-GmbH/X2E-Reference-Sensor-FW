//==========================================================================
//==========================================================================
#ifndef INC_SENSOR_H
#define INC_SENSOR_H

//==========================================================================
//==========================================================================
#include "mx_target.h"

//==========================================================================
//==========================================================================
typedef struct {
  double pressure;     // in hPa
  double temperature;  // in deg C
  double humidity;     // in %RH
  float iaq;           // IAQ index.
  float tvoc;          // TVOC concentration (mg/m^3).
  float etoh;          // EtOH concentration (ppm).
  float eco2;          // eCO2 concentration (ppm).
} SensorResult_t;

//==========================================================================
//==========================================================================
int8_t SensorInit(void);
void SensorPrepareForSleep(void);
void SensorResumeFromSleep(void);

void SensorStartMeas(bool aEnableAirQuality);
bool SensorIsReady(void);
uint32_t SensorMeasInterval(void);

void SensorGetBattery(float *aPrecentage, float *aCurrent, float *aVoltage);
void SensorGetResult(SensorResult_t *aResult);

//==========================================================================
//==========================================================================
#endif  // INC_SENSOR_OP_H