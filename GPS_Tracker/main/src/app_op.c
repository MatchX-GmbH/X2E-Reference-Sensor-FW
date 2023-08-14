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
#include "lora_compon.h"
#include "matchx_payload.h"
#include "task_priority.h"
#include "packer.h"
#include "led.h"
#include "sensors.h"
#include "GPS_Distance.h"
#include "esp_sleep.h"
#include "sleep.h"
#include "driver/gpio.h"

//==========================================================================
// Defines
//==========================================================================
#define DelayMs(x) vTaskDelay(x / portTICK_PERIOD_MS)

// config
#if defined(CONFIG_MATCHX_ENABLE_DEEP_SLEEP)
#define MATCHX_ENABLE_DEEP_SLEEP true
#else
#define MATCHX_ENABLE_DEEP_SLEEP false
#endif

#define USR_BUTTON GPIO_NUM_0
#define ACCEL_INT GPIO_NUM_3

// LoRa Interval and Sleep Threshold in ms
#define TIME_GO_TO_SLEEP_THRESHOLD 2000
#define TIME_DEEP_SLEEP_THRESHOLD 30000
#define INTERVAL_SENDING_DATA 120000

// States
typedef enum {
  S_IDLE = 0,
  S_SAMPLE_DATA,
  S_SEND_DATA,
  S_SEND_DATA_WAIT,
  S_WAIT_INTERVAL,
} LoRaState_t;

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gAppOpHandle = NULL;

//==========================================================================
// Send data to LoRa
//==========================================================================
//static void SendData(void) {
//  float voltage0 = 3.21;
//  uint8_t buf[8];
//  uint8_t *ptr;
//
//  ptr = buf;
//  ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
//  ptr += PackU8(ptr, MX_SENSOR_VOLTAGE);
//  ptr += PackFloat(ptr, voltage0);
//  uint16_t tx_len = ptr - buf;
//
//  Hex2String("Sending ", buf, tx_len);
//  LoRaComponSendData(buf, tx_len);
//}

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
static void EnterSleep(uint32_t aTimeToSleep, bool aDeepSleep) {
  // Call all to prepare for sleep

  LoRaComponPrepareForSleep(aDeepSleep);
  MxTargetPrepareForSleep();

  // Enter sleep
  if (aDeepSleep) {
    EnterDeepSleep(aTimeToSleep, true);
  } else {
    EnterLightSleep(aTimeToSleep, true);
  }

  // Wake up and call all to resume
  MxTargetResumeFromSleep();
  LoRaComponResumeFromSleep();
  if (IsWakeByEXT1() || gpio_get_level(ACCEL_INT)) {
    SensorResumeFromSleep();
  }
}

//==========================================================================
//==========================================================================
static void AppOpTask(void *param) {
  uint32_t interval = INTERVAL_SENDING_DATA;
  bool wake_from_sleep = false;
  uint8_t buf[8] = {
      0 };

  PrintLine("GPS Tracker (%s).", LoRaComponRegionName());

  //
  if (IsWakeByReset()) {
    wake_from_sleep = false;
  } else {
    DEBUG_PRINTLINE("Wake up from sleep");
    wake_from_sleep = true;
  }

  // Kick start the LoRa component
  LoRaComponStart(wake_from_sleep);
  PrintLine("LoRa component started.");

  // start main loop of AppOp.
  uint32_t tick_lora;
  LoRaState_t state_lora = S_IDLE;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    switch (state_lora) {
      case S_IDLE:
        if (LoRaComponIsJoined()) {
//        LedSet(LED_MODE_ON, -1);
          if (LoRaComponIsIsm2400()) {
            PrintLine("Joined with ISM2400");
          } else {
            PrintLine("Joined with sub-GHz");
          }
          PrintLine("Data sending interval %ds.", interval / 1000);
          tick_lora = GetTick();
          state_lora = S_SAMPLE_DATA;
        } else {
//        LedSet(LED_MODE_SLOW_BLINKING, -1);
          if (1) {   // Add this condition later (IsExtPower() == false) to keep awake while external power
            uint32_t time_to_sleep = LoRaComponGetWaitingTime();
            if ((time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) && SensorSleepReady()) {
              DEBUG_PRINTLINE("S_IDLE time_to_sleep=%u", time_to_sleep);
              if ((MATCHX_ENABLE_DEEP_SLEEP) && (time_to_sleep >= TIME_DEEP_SLEEP_THRESHOLD)) {
                EnterSleep(time_to_sleep, true);
              } else {
                EnterSleep(time_to_sleep, false);
              }
            }
          }
        }
        break;
      case S_SAMPLE_DATA:
        if (TickElapsed(tick_lora) >= 5000) {
          state_lora = S_SEND_DATA;
        }
        break;
      case S_SEND_DATA:
//        LedSet(LED_MODE_FAST_BLINKING, -1);

        // Reporting battery status
        BatteryStatusUpdate();

        // Send Data if available
        if (GPS_SendData() != 0) {
          state_lora = S_SEND_DATA_WAIT;
        } else {
          LoRaComponSendData(buf, 1);   // We can send Blank Frame to report the Battery status
          state_lora = S_SEND_DATA_WAIT;
//          state_lora = S_WAIT_INTERVAL;
        }
        tick_lora = GetTick();
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
        } else if (1) {   // Add this condition later (IsExtPower() == false) to keep awake while external power
          uint32_t time_to_sleep = LoRaComponGetWaitingTime();

          if ((time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) && SensorSleepReady()) {
            DEBUG_PRINTLINE("S_SEND_DATA_WAIT time_to_sleep=%u", time_to_sleep);
            EnterSleep(time_to_sleep, false);
          }
        }
        break;
      case S_WAIT_INTERVAL:
//      LedSet(LED_MODE_ON, -1);
        if (LoRaComponIsRxReady()) {
          GetData();
        }
        uint32_t elapsed = TickElapsed(tick_lora);

        if (elapsed >= interval) {
          tick_lora = GetTick();
          state_lora = S_SAMPLE_DATA;
        } else if (1) {   // Add this condition later (IsExtPower() == false) to keep awake while external power
          uint32_t lora_waiting_time = LoRaComponGetWaitingTime();

          uint32_t time_to_sleep = interval - elapsed;
          if (lora_waiting_time < time_to_sleep)
            time_to_sleep = lora_waiting_time;
          if ((time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) && SensorSleepReady()) {
            DEBUG_PRINTLINE("S_WAIT_INTERVAL time_to_sleep=%u", time_to_sleep);
            if ((MATCHX_ENABLE_DEEP_SLEEP) && (time_to_sleep >= TIME_DEEP_SLEEP_THRESHOLD)) {
              EnterSleep(time_to_sleep, true);
            } else {
              EnterSleep(time_to_sleep, false);
            }
          }
        }
        break;

      default:
        state_lora = S_IDLE;
        break;
    }
    // Check connection
    if ((state_lora != S_IDLE) && (!LoRaComponIsJoined())) {
      PrintLine("Connection lost.");
      state_lora = S_IDLE;
    }

    // Check Movement
    if (SensorSleepReady() && gpio_get_level(ACCEL_INT)) {
      SensorResumeFromSleep();
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

  LedInit();
  LedSet(LED_MODE_SLOW_BLINKING, 2, 1);

  // Sensors Init
  if (SensorsInit() != ESP_OK) {
    printf("ERROR. Failed to Initialize Sensors.\n");
    return -1;
  }

  DelayMs(5000);
  LedSet(LED_MODE_FAST_BLINKING, 10, 0);
  DelayMs(5000);

// Create tasks
  gSensorTaskHandle = NULL;
  if (xTaskCreate(SensorsTask, "Sensors", 4096, NULL, TASK_PRIO_GENERAL, &gSensorTaskHandle) != pdPASS) {
    printf("ERROR. Failed to create Sensors task.\n");
    return -1;
  }

  if (xTaskCreate(AppOpTask, "AppOp", 4096, NULL, TASK_PRIO_GENERAL, &gAppOpHandle) != pdPASS) {
    printf("ERROR. Failed to create AppOp task.\n");
    return -1;
  }
  return 0;
}
