//==========================================================================
//==========================================================================
#ifndef INC_AIR_QUALITY_H
#define INC_AIR_QUALITY_H

//==========================================================================
//==========================================================================
#include <math.h>
#include "mx_target.h"

//==========================================================================
//==========================================================================
typedef struct {
  bool valid;
  bool warmUp;
  float iaq;   // IAQ index.
  float tvoc;  // TVOC concentration (mg/m^3).
  float etoh;  // EtOH concentration (ppm).
  float eco2;  // eCO2 concentration (ppm).
} AirQualityResult_t;

//==========================================================================
//==========================================================================
int8_t AirQualityInit(void);
int8_t AirQualityRead(void);
int8_t AirQualityStartMeas(void);
bool AirQualityIsMeasDone(void);
int8_t AirQualityGetResult(AirQualityResult_t *aResult, float aTemp, float aHumi);
uint32_t AirQualityMeasTime(void);
uint32_t AirQualityMeasInterval(void);

//==========================================================================
//==========================================================================
#endif  // INC_AIR_QUALITY_H