//==========================================================================
// Sensor task
//==========================================================================
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
// Naming conventions
// ~~~~~~~~~~~~~~~~~~
//                Class : Leading C
//               Struct : Leading T
//       typedef Struct : tailing _t
//             Constant : Leading k
//      Global Variable : Leading g
//    Function argument : Leading a
//       Local Variable : All lower case
//==========================================================================
#include "sensor.h"

#include <math.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "battery.h"
#include "mutex_helper.h"
#include "task_priority.h"

//==========================================================================
// Config
//==========================================================================
#if defined(CONFIG_MATCHX_BATTERY_DESING_CAPACITY)
#define MATCHX_BATTERY_DESING_CAPACITY CONFIG_MATCHX_BATTERY_DESING_CAPACITY
#else
#define MATCHX_BATTERY_DESING_CAPACITY 1200
#endif

#define BATTERY_DESING_VOLTAGE_mV 3700

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gSensorTaskHandle = NULL;
static BatteryResult_t gBatteryResult;

//==========================================================================
// Set battery design values
//==========================================================================
static void SetBatteryDesignValues(void) {
  uint16_t design_capacity;
  uint16_t design_voltage_mv;

  // Set design capacity and voltage
  if (BatteryGetDesignValue(&design_capacity, &design_voltage_mv) < 0) {
    PrintLine("BatteryGetDesignValue failed. Please try power off/on again.");
    // Uncondition set
    BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV);
    return;
  }
  DEBUG_PRINTLINE("Battery design values: %dmAh %dmV", design_capacity, design_voltage_mv);

  if ((design_capacity != MATCHX_BATTERY_DESING_CAPACITY) || (design_voltage_mv != BATTERY_DESING_VOLTAGE_mV)) {
    if (BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV) < 0) {
      PrintLine("Change design capacity failed.");
      return;
    }
    PrintLine("INFO. Change battery design values to %dmAh %dmV success.", MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV);
  } else {
    PrintLine("INFO. Battery design values: %dmAh %dmV", design_capacity, design_voltage_mv);
  }
}

//==========================================================================
//==========================================================================
static void SensorTask(void *param) {
  // Battery
  BatteryInit();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  SetBatteryDesignValues();
  // BatteryGetResult(&gBatteryResult);
  // if (gBatteryResult.designCapacity != MATCHX_BATTERY_DESING_CAPACITY) {
  //   if (BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV)) {
  //     PrintLine("INFO. Change design capacity to %dmAh %.2fV success.", MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV);
  //   } else {
  //     PrintLine("WARNING. Change design capacity failed.");
  //   }
  // }

  // start main loop of task
  for (;;) {
    //
    if (TakeMutex()) {
      BatteryGetResult(&gBatteryResult);
      FreeMutex();
      // DEBUG_PRINTLINE("Battery %.2fV %.2fA", gBatteryInfo.voltage, gBatteryInfo.current);
    }

    vTaskDelay(1500 / portTICK_PERIOD_MS);
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t SensorInit(void) {
  InitMutex();

  // Create task
  if (xTaskCreate(SensorTask, "Sensor", 4096, NULL, TASK_PRIO_GENERAL, &gSensorTaskHandle) != pdPASS) {
    PrintLine("ERROR. Failed to create Sensor task.");
    return -1;
  } else {
    return 0;
  }
}

//==========================================================================
// Sleep
//==========================================================================
void SensorPrepareForSleep(void) {
  if (gSensorTaskHandle != NULL) {
    vTaskSuspend(gSensorTaskHandle);
  }
}

void SensorResumeFromSleep(void) {
  if (gSensorTaskHandle != NULL) {
    vTaskResume(gSensorTaskHandle);
  }
}

//==========================================================================
// Get sensor data
//==========================================================================
void SensorGetBattery(float *aPrecentage, float *aCurrent, float *aVoltage) {
  *aPrecentage = NAN;
  *aCurrent = NAN;
  *aVoltage = NAN;
  if (TakeMutex()) {
    *aPrecentage = gBatteryResult.percentage;
    *aCurrent = gBatteryResult.current;
    *aVoltage = gBatteryResult.voltage;
    FreeMutex();
  }
}
