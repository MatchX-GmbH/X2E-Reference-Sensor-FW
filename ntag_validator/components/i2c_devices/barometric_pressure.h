//==========================================================================
//==========================================================================
#ifndef INC_BAROMERIC_PRESSURE_H
#define INC_BAROMERIC_PRESSURE_H

//==========================================================================
//==========================================================================
#include "mx_target.h"

//==========================================================================
//==========================================================================
typedef struct {
  bool valid;
  double pressure;      // in hPa
  double temperature;   // in deg C
  double humidity;      // in %RH
} BarometricPressureResult_t;

//==========================================================================
//==========================================================================
int8_t BarometricPressureInit(void);
int8_t BarometricPressureStartMeas(void);
bool BarometricPressureIsMeasDone(void);
int8_t BarometricPressureGetResult(BarometricPressureResult_t *aResult);
uint32_t BarometricPressureMeasTime (void);

//==========================================================================
//==========================================================================
#endif  // INC_BAROMERIC_PRESSURE_H