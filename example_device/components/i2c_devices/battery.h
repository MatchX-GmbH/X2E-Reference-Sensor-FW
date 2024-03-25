//==========================================================================
//==========================================================================
#ifndef INC_BATTERY_H
#define INC_BATTERY_H

//==========================================================================
//==========================================================================
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "mx_target.h"

//==========================================================================
//==========================================================================
typedef struct {
  bool valid;
  float voltage;
  float current;
  float percentage;
  uint16_t designCapacity;
  uint16_t fullChargeCapacity;
  uint16_t remainingCapacity;
} BatteryResult_t;

//==========================================================================
//==========================================================================
void BatteryInit(void);
int8_t BatteryGetResult(BatteryResult_t *aResult);
int8_t BatterySetDesignValue(uint16_t aCapacity, uint16_t aVoltage_mV);
int8_t BatteryGetDesignValue(uint16_t *aCapacity, uint16_t *aVoltage_mV);

//==========================================================================
//==========================================================================
#endif  // INC_BATTERY_H