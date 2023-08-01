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
#include "i2c_sensors/battery.h"
#include "i2c_sensors/lis2de12.h"
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

#define BATTERY_DESING_VOLTAGE 3.7

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gSensorTaskHandle = NULL;
static lis2de12_config_t gAccelConfig;
static BatteryInfo_t gBatteryInfo;
static lis2de12_acce_value_t gAccelResult;

//==========================================================================
//==========================================================================
static void SensorTask(void *param) {
  // Accelerometer
  memcpy(&gAccelConfig, &((lis2de12_config_t)LIS2DE12_CONFIG_DEFAULT()), sizeof(lis2de12_config_t));

  gAccelConfig.fs = LIS2DE12_4g;
  gAccelConfig.odr = LIS2DE12_ODR_10Hz;
  gAccelConfig.fds = LIS2DE12_DISABLE;

  lis2de12_initialization(I2CNUM, &gAccelConfig);
  gAccelResult.acce_x = NAN;
  gAccelResult.acce_y = NAN;
  gAccelResult.acce_z = NAN;

  // Battery
  BatteryInit();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  BatteryProcess();
  BatteryGetInfo(&gBatteryInfo);
  if (gBatteryInfo.designCapacity != MATCHX_BATTERY_DESING_CAPACITY) {
    if (BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE)) {
      PrintLine("Change design capacity to %dmAh %.2fV success.", MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE);
    } else {
      PrintLine("Change design capacity failed.");
    }
  }

  // start main loop of task
  for (;;) {
    //
    BatteryProcess();
    if (TakeMutex()) {
      BatteryGetInfo(&gBatteryInfo);
      FreeMutex();
      // DEBUG_PRINTLINE("Battery %.2fV %.2fA", gBatteryInfo.voltage, gBatteryInfo.current);
    }

    // Accelerometer
    if (lis2de12_Is_data_ready()) {
      if (TakeMutex()) {
        lis2de12_get_acce(&gAccelResult);
        FreeMutex();
        // DEBUG_PRINTLINE("x=%.2fg, y=%.2fg, z=%.2fg", gAccelResult.acce_x / 1000, gAccelResult.acce_y / 1000,
        // gAccelResult.acce_z / 1000);
      }
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
    printf("ERROR. Failed to create Sensor task.\n");
    return -1;
  } else {
    return 0;
  }
}

//==========================================================================
// Sleep
//==========================================================================
void SensorPrepareForSleep(void) {
  vTaskSuspend(gSensorTaskHandle);
}

void SensorResumeFromSleep(void) {
  vTaskResume(gSensorTaskHandle);
}

//==========================================================================
// Get sensor data
//==========================================================================
void SensorGetBattery(float *aPrecentage, float *aCurrent, float *aVoltage) {
  *aPrecentage = NAN;
  *aCurrent = NAN;
  *aVoltage = NAN;
  if (TakeMutex()) {
    *aPrecentage = gBatteryInfo.percentage;
    *aCurrent = gBatteryInfo.current;
    *aVoltage = gBatteryInfo.voltage;
    FreeMutex();
  }
}
