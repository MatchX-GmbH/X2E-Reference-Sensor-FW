//==========================================================================
// Application
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
#include "app_op.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "led.h"
#include "lora_compon.h"
#include "matchx_payload.h"
#include "packer.h"
#include "sensor.h"
#include "sleep.h"
#include "task_priority.h"

//==========================================================================
// Defines
//==========================================================================
#define DelayMs(x) vTaskDelay(x / portTICK_PERIOD_MS)

// States
typedef enum {
  S_IDLE = 0,
  S_JOIN_WAIT,
  S_SAMPLE_DATA,
  S_SEND_DATA,
  S_SEND_DATA_WAIT,
  S_WAIT_INTERVAL,
} LoRaState_t;

// Battery Current threshold for external power detection
#define VALUE_EXT_POWER_CURRENT_THRESHOLD -0.01

// ms
#define TIME_GO_TO_SLEEP_THRESHOLD  2000

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gAppOpHandle = NULL;

//==========================================================================
// Check external power
//==========================================================================
static bool IsExtPower(void) {
  float batt_voltage;
  float batt_current;
  float batt_percentage;
  
  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);

  //
  if (batt_current >= VALUE_EXT_POWER_CURRENT_THRESHOLD) {
    return true;
  } else {
    return false;
  }
}

//==========================================================================
// Send data to LoRa
//==========================================================================
static void SendData(void) {
  float batt_voltage;
  float batt_current;
  float batt_percentage;
  uint8_t buf[8];
  uint8_t *ptr;

  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);

  //
  if (batt_current >= VALUE_EXT_POWER_CURRENT_THRESHOLD) {
    // Set reporting battery status to external power.
    LoRaComponSetExtPower();
  } else {
    LoRaComponSetBatteryPercent(batt_percentage);
  }

  ptr = buf;
  ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
  ptr += PackU8(ptr, MX_SENSOR_VOLTAGE);
  ptr += PackFloat(ptr, batt_voltage);
  ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
  ptr += PackU8(ptr, MX_SENSOR_CURRENT);
  ptr += PackFloat(ptr, batt_current);

  uint16_t tx_len = ptr - buf;

  Hex2String("Sending ", buf, tx_len);
  LoRaComponSendData(buf, tx_len);
}

//==========================================================================
// Get Data from LoRa
//==========================================================================
static void GetData(void) {
  uint8_t buf[256];
  LoRaRxInfo_t info;

  int32_t rx_len = LoRaComponGetData(buf, sizeof(buf), &info);
  if (rx_len < 0) {
    PrintLine("Rx error.");
  } else {
    PrintLine("Rxed %d bytes. fport=%d, rssi=%d, datarate=%d", rx_len, info.fport, info.rssi, info.datarate);
    Hex2String("  ", buf, rx_len);
  }
}

//==========================================================================
// Sleep
//==========================================================================
static void EnterSleep(uint32_t aTimeToSleep) {
  // Call all to prepare for sleep
  SensorPrepareForSleep();
  LoRaComponPrepareForSleep();
  MxTargetPrepareForSleep();

  // Enter sleep
  EnterLightSleep(aTimeToSleep, false);

  // Wake up and call all to resume
  MxTargetResumeFromSleep();
  LoRaComponResumeFromSleep();
  SensorResumeFromSleep();
}

//==========================================================================
//==========================================================================
static void AppOpTask(void *param) {
  uint32_t interval = 120000;

  PrintLine("Example Device (%s).", LoRaComponRegionName());

  // Kick start the LoRa component
  LoRaComponStart();
  PrintLine("LoRa component started.");

  // start main loop of AppOp.
  uint32_t tick_lora;
  LoRaState_t state_lora = S_IDLE;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    //
    switch (state_lora) {
      case S_IDLE:
        if (LoRaComponIsJoined()) {
          LedSet(true);
          if (LoRaComponIsIsm2400()) {
            PrintLine("Joined with ISM2400");
          } else {
            PrintLine("Joined with sub-GHz");
          }
          PrintLine("Data sending interval %ds.", interval / 1000);
          tick_lora = GetTick();
          state_lora = S_JOIN_WAIT;
        } else {
          LedSet(false);
          if (IsExtPower() == false) {
            uint32_t time_to_sleep = LoRaComponGetWaitingTime();
            if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
              DEBUG_PRINTLINE("S_IDLE time_to_sleep=%u", time_to_sleep);
              EnterSleep(time_to_sleep);
            }
          }
        }
        break;
      case S_JOIN_WAIT:
        if (TickElapsed(tick_lora) >= 5000) {
          state_lora = S_SEND_DATA;
        }
        break;
      case S_SAMPLE_DATA:
        if (TickElapsed(tick_lora) >= 5000) {
          state_lora = S_SEND_DATA;
        }
        break;
      case S_SEND_DATA:
        // Send data
        SendData();
        tick_lora = GetTick();
        state_lora = S_SEND_DATA_WAIT;
        break;
      case S_SEND_DATA_WAIT:
        if (LoRaComponIsSendDone()) {
          if (LoRaComponIsSendSuccess()) {
            PrintLine("Send data success.");
          } else {
            PrintLine("Send data failed.");
          }
          tick_lora = GetTick();
          state_lora = S_WAIT_INTERVAL;
        } else if (IsExtPower() == false) {
          uint32_t time_to_sleep = LoRaComponGetWaitingTime();

          if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
            DEBUG_PRINTLINE("S_SEND_DATA_WAIT time_to_sleep=%u", time_to_sleep);
            EnterSleep(time_to_sleep);
          }
        }
        break;
      case S_WAIT_INTERVAL: {
        if (LoRaComponIsRxReady()) {
          GetData();
        }
        uint32_t elapsed = TickElapsed(tick_lora);
        if (elapsed >= interval) {
          tick_lora = GetTick();
          state_lora = S_SAMPLE_DATA;
        } else if (IsExtPower() == false) {
          uint32_t time_to_sleep = interval - elapsed;
          uint32_t lora_waiting_time = LoRaComponGetWaitingTime();

          if (lora_waiting_time < time_to_sleep) time_to_sleep = lora_waiting_time;
          if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
            DEBUG_PRINTLINE("S_WAIT_INTERVAL time_to_sleep=%u", time_to_sleep);
            EnterSleep(time_to_sleep);
          }
        }
        break;
      }

      default:
        state_lora = S_IDLE;
        break;
    }
    // Check connection
    if ((state_lora != S_IDLE) && (!LoRaComponIsJoined())) {
      PrintLine("Connection lost.");
      state_lora = S_IDLE;
    }
  }
  LoRaComponStop();

  //
  PrintLine("AppOpTask ended.");
  gAppOpHandle = NULL;
  vTaskDelete(NULL);
}

//==========================================================================
// Init
//==========================================================================
int8_t AppOpInit(void) {
  MxTargetInit();

  // LoRa hardware related init
  LoRaComponHwInit();

  //
  LedInit();
  LedSet(false);
  SensorInit();


  // Create task
  if (xTaskCreate(AppOpTask, "AppOp", 4096, NULL, TASK_PRIO_GENERAL, &gAppOpHandle) != pdPASS) {
    printf("ERROR. Failed to create AppOp task.\n");
    return -1;
  } else {
    return 0;
  }
}
