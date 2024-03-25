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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app_data.h"
#include "app_utils.h"
#include "ble_dfu.h"
#include "button.h"
#include "debug.h"
#include "led.h"
#include "lora_compon.h"
#include "matchx_payload.h"
#include "packer.h"
#include "sensor.h"
#include "sha256.h"
#include "sleep.h"
#include "task_priority.h"

#define JSMN_HEADER
#include "jsmn.h"

//==========================================================================
// Defines
//==========================================================================
#define DelayMs(x) vTaskDelay(x / portTICK_PERIOD_MS)

// config
#if defined(CONFIG_MATCHX_DISABlE_SLEEP)
#define MATCHX_DISABlE_SLEEP true
#define MATCHX_ENABLE_DEEP_SLEEP false
#elif defined(CONFIG_MATCHX_ENABLE_DEEP_SLEEP)
#define MATCHX_DISABlE_SLEEP false
#define MATCHX_ENABLE_DEEP_SLEEP true
#else
#define MATCHX_DISABlE_SLEEP false
#define MATCHX_ENABLE_DEEP_SLEEP false
#endif

// States
typedef enum {
  S_IDLE = 0,
  S_WAIT_JOIN,
  S_MEASUREMENT_START,
  S_MEASUREMENT_WAIT,
  S_MEASUREMENT_INTERVAL,
  S_SEND_DATA,
  S_SEND_DATA_WAIT,
  S_WAIT_INTERVAL,
} LoRaState_t;

// Battery Current threshold for external power detection
#define VALUE_EXT_POWER_CURRENT_THRESHOLD -0.005

// ms
#define TIME_GO_TO_SLEEP_THRESHOLD 2000
#define TIME_DEEP_SLEEP_THRESHOLD 30000
#define INTERVAL_SENDING_DATA_DC (5 * 60 * 1000)
#define INTERVAL_SENDING_DATA_BATT (60 * 60 * 1000)
#define TIME_WAIT_DFU 60000

//==========================================================================
// Constants
//==========================================================================
// PID
static const char *kPidHashSuffix = ".MatchX";
static char gPid[32];

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
  // DEBUG_PRINTLINE("Battery %.3fA %.2fV", batt_current, batt_voltage);

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
  SensorResult_t result;

  uint8_t buf[32];
  uint8_t *ptr;

  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);
  SensorGetResult(&result);

  //
  if (batt_current >= VALUE_EXT_POWER_CURRENT_THRESHOLD) {
    // Set reporting battery status to external power.
    LoRaComponSetExtPower();
  } else {
    LoRaComponSetBatteryPercent(batt_percentage);
  }

  ptr = buf;
  if (!isnan(result.pressure)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_PRESSURE);
    ptr += PackFloat(ptr, result.pressure);
  }
  if (!isnan(result.temperature)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_TEMPERATURE);
    ptr += PackFloat(ptr, result.temperature);
  }
  if (!isnan(result.humidity)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_HUMIDITY);
    ptr += PackFloat(ptr, result.humidity);
  }
  if (!isnan(result.iaq)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_IAQ);
    ptr += PackFloat(ptr, result.iaq);
  }
  if (!isnan(result.tvoc)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_TVOC);
    ptr += PackFloat(ptr, result.tvoc);
  }
  if (!isnan(result.eco2)) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
    ptr += PackU8(ptr, MX_SENSOR_CO2);
    ptr += PackFloat(ptr, result.eco2);
  }

  uint16_t tx_len = ptr - buf;

  Hex2String("INFO. Sending ", buf, tx_len);
  LoRaComponSendData(buf, tx_len);
}

static void SendBlankFrame(void) {
  float batt_voltage;
  float batt_current;
  float batt_percentage;
  uint8_t buf[1];

  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);

  //
  if (batt_current >= VALUE_EXT_POWER_CURRENT_THRESHOLD) {
    // Set reporting battery status to external power.
    LoRaComponSetExtPower();
  } else {
    LoRaComponSetBatteryPercent(batt_percentage);
  }
  LoRaComponSendData(buf, 0);
}

//==========================================================================
// Get Data from LoRa
//==========================================================================
static void GetData(void) {
  uint8_t buf[256];
  LoRaRxInfo_t info;

  int32_t rx_len = LoRaComponGetData(buf, sizeof(buf), &info);
  if (rx_len < 0) {
    PrintLine("ERROR. LoRaComponGetData failed.");
  } else {
    PrintLine("INFO. Rxed %d bytes. fport=%d, rssi=%d, datarate=%d", rx_len, info.fport, info.rssi, info.datarate);
    Hex2String("  ", buf, rx_len);
  }
}

//==========================================================================
// Sleep
//==========================================================================
static void EnterSleep(uint32_t aTimeToSleep, bool aDeepSleep) {
  // Call all to prepare for sleep
  SensorPrepareForSleep();
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
  SensorResumeFromSleep();
}

//==========================================================================
// JSON helper
//==========================================================================
static char gSearchJsonKey[64];
static char gSearchJsonValue[64];

static void copyJsonToken(char *aDest, int aDestSize, const char *aJson, const jsmntok_t *aToken) {
  int len = aToken->end - aToken->start;
  if (len > (aDestSize - 1)) len = aDestSize - 1;
  strncpy(aDest, aJson + aToken->start, len);
  aDest[len] = 0;
}

static int getJsonString(const char *aJson, const jsmntok_t *aToken, int aTokenCount, const char *aKey, char *aValue,
                         size_t aValueSize) {
  memset(aValue, 0, aValueSize);
  for (int i = 1; i < aTokenCount; i++) {
    copyJsonToken(gSearchJsonKey, sizeof(gSearchJsonKey), aJson, &aToken[i]);

    if ((aToken[i].type == JSMN_STRING) && (strncmp(aKey, gSearchJsonKey, sizeof(gSearchJsonKey)) == 0)) {
      if (aToken[i + 1].type == JSMN_STRING) {
        copyJsonToken(gSearchJsonValue, sizeof(gSearchJsonValue), aJson, &aToken[i + 1]);
        strncpy(aValue, gSearchJsonValue, aValueSize - 1);
      }
      return 0;
    }
  }
  PrintLine("WARNING. JSON Token %s not found.", aKey);
  return -1;
}

//==========================================================================
// Kick start LoRa
//==========================================================================
static void KickStartLoRa(bool aWakeFromSleep) {
  uint8_t pid_hash[SHA256_BLOCK_SIZE];
  AppSettings_t *settings = AppSettingsGet();
  bool msg_shown = false;

  // Wait until PID is ready
  for (;;) {
    // Try read PID from AppSettings
    memset(gPid, 0, sizeof(gPid));
    memset(pid_hash, 0, sizeof(pid_hash));
    if (AppDataTakeMutex()) {
      if (strlen(settings->qrCode) > 0) {
        jsmn_parser j_parser;
        jsmntok_t j_tokens[16];

        jsmn_init(&j_parser);

        int j_ret =
            jsmn_parse(&j_parser, settings->qrCode, strlen(settings->qrCode), j_tokens, sizeof(j_tokens) / sizeof(jsmntok_t));
        if (j_ret < 0) {
          PrintLine("ERROR. QRCODE JSON resp parse error.");
        } else if ((j_ret < 1) || (j_tokens[0].type != JSMN_OBJECT)) {
          PrintLine("ERROR. QRCODE JSON resp invalid format.");
        } else {
          if (getJsonString(settings->qrCode, j_tokens, j_ret, "PID", gPid, sizeof(gPid)) == 0) {
            SHA256_CTX ctx;

            sha256_init(&ctx);
            sha256_update(&ctx, (const uint8_t *)gPid, strlen(gPid));
            sha256_update(&ctx, (const uint8_t *)kPidHashSuffix, strlen(kPidHashSuffix));
            sha256_final(&ctx, pid_hash);

            DEBUG_PRINTLINE("PID: %s", gPid);
            DEBUG_HEX2STRING("HASH: ", pid_hash, sizeof(pid_hash));
          } else {
            memset(gPid, 0, sizeof(gPid));
            memset(pid_hash, 0, sizeof(pid_hash));
          }
        }
      }
      AppDataFreeMutex();
    }

    // Got PID, kick start LoRa and return
    if (strlen(gPid) > 0) {
      LoRaComponStart(gPid, pid_hash, aWakeFromSleep);
      PrintLine("INFO. LoRa component started.");
      break;
    }

    //
    if (!msg_shown) {
      PrintLine("WARNING. PID not found. Waiting...");
      msg_shown = true;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

//==========================================================================
//==========================================================================
static void AppOpTask(void *param) {
  bool wake_from_sleep;

  PrintLine("INFO. Environmental Sensor (%s).", LoRaComponRegionName());

  //
  if (IsWakeByReset()) {
    wake_from_sleep = false;
  } else {
    DEBUG_PRINTLINE("Wake up from sleep");
    wake_from_sleep = true;
  }

  //
  KickStartLoRa(wake_from_sleep);
  if (!LoRaComponIsProvisioned()) {
    PrintLine("INFO. Waiting for provision.");
  }

  // start main loop of AppOp.
  uint32_t tick_lora;
  uint32_t tick_measurement;
  uint32_t tick_dfu = 0;
  LoRaState_t state_lora = S_IDLE;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Button and BLE DFU
    if (ButtonIsPressed()) {
      if (ButtonIsHeld(5000)) {
        tick_dfu = GetTick();
        if (!BleDfuServerIsStarted()) {
          char dfu_name[20];
          char *ptr = gPid;
          if (strlen(gPid) > 5) {
            ptr += (strlen(gPid) - 5);
          }
          snprintf(dfu_name, sizeof(dfu_name), "X2E_DFU_%s", ptr);
          if (BleDfuServerStart(dfu_name) >= 0) {
            PrintLine("INFO. BLE DFU started.");
          }
        }
        // Wait button release
        LedSet(LED_MODE_FAST_BLINKING, -1);
        for (;;) {
          if (!ButtonIsPressed()) break;
          vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        ButtonClear();
      }
    }
    if (BleDfuServerIsConnected()) {
      LedSet(LED_MODE_FAST_BLINKING, -1);
      PrintLine("INFO. BLE connected.");
      LoRaComponStop();
      // Wait disconnect
      for (;;) {
        if (!BleDfuServerIsConnected()) break;
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      // Restart
      PrintLine("INFO. BLE disconnected.");
      vTaskDelay(500 / portTICK_PERIOD_MS);
      esp_restart();
    } else if ((BleDfuServerIsStarted()) && (TickElapsed(tick_dfu) > TIME_WAIT_DFU)) {
      BleDfuServerStop();
      PrintLine("INFO. BLE DFU stopped.");
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    //
    bool ext_power = IsExtPower();
    bool allow_sleep = false;
    if ((!MATCHX_DISABlE_SLEEP) && (!ext_power) && (!BleDfuServerIsStarted()) && (!ButtonIsPressed())) {
      allow_sleep = true;
    }

    //

    //
    switch (state_lora) {
      case S_IDLE:
        if (LoRaComponIsProvisioned()) {
          PrintLine("INFO. Device is provisioned.");
          state_lora = S_WAIT_JOIN;
        } else {
          LedSet(LED_MODE_SHORT_PULSE, -1);
        }
        break;
      case S_WAIT_JOIN:
        if (LoRaComponIsJoined()) {
          LedSet(LED_MODE_ON, -1);
          if (LoRaComponIsIsm2400()) {
            PrintLine("INFO. Joined with ISM2400");
          } else {
            PrintLine("INFO. Joined with sub-GHz");
          }
          PrintLine("INFO. Data sending interval, DC=%ds, BATT=%ds", INTERVAL_SENDING_DATA_DC / 1000,
                    INTERVAL_SENDING_DATA_BATT / 1000);
          tick_lora = 0;
          state_lora = S_MEASUREMENT_START;
        } else {
          LedSet(LED_MODE_SLOW_BLINKING, -1);
          if (allow_sleep) {
            uint32_t time_to_sleep = LoRaComponGetWaitingTime();
            if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
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
      case S_MEASUREMENT_START:
        tick_measurement = GetTick();
        SensorStartMeas(ext_power);
        state_lora = S_MEASUREMENT_WAIT;
        break;
      case S_MEASUREMENT_WAIT:
        if (SensorIsReady()) {
          uint32_t interval = INTERVAL_SENDING_DATA_BATT;
          if (ext_power) interval = INTERVAL_SENDING_DATA_DC;
          if ((tick_lora == 0) || (TickElapsed(tick_lora) > interval)) {
            state_lora = S_SEND_DATA;
          } else {
            state_lora = S_MEASUREMENT_INTERVAL;
          }
        }
        break;
      case S_MEASUREMENT_INTERVAL:
        LedSet(LED_MODE_ON, -1);
        if (LoRaComponIsRxReady()) {
          GetData();
        }
        if (TickElapsed(tick_measurement) >= SensorMeasInterval()) {
          state_lora = S_MEASUREMENT_START;
        } else {
          // Determine sleep time
          uint32_t time_to_sleep = SensorMeasInterval();
          uint32_t time_lore_waiting = LoRaComponGetWaitingTime();
          uint32_t measure_time = TickElapsed(tick_measurement);
          if (!ext_power) {
            time_to_sleep = INTERVAL_SENDING_DATA_BATT;
          }
          if (time_to_sleep > measure_time) {
            time_to_sleep -= measure_time;
          }
          if (time_to_sleep > time_lore_waiting) {
            time_to_sleep = time_lore_waiting;
          }
          if ((allow_sleep) && (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD)) {
            DEBUG_PRINTLINE("S_MEASUREMENT_INTERVAL time_to_sleep=%u", time_to_sleep);
            if ((MATCHX_ENABLE_DEEP_SLEEP) && (time_to_sleep >= TIME_DEEP_SLEEP_THRESHOLD)) {
              EnterSleep(time_to_sleep, true);
            } else {
              EnterSleep(time_to_sleep, false);
            }
          }
        }
        break;

      case S_SEND_DATA:
        // Send data
        LedSet(LED_MODE_BLINKING, -1);
        SendData();
        tick_lora = GetTick();
        state_lora = S_SEND_DATA_WAIT;
        break;
      case S_SEND_DATA_WAIT:
        if (LoRaComponIsSendDone()) {
          if (LoRaComponIsSendSuccess()) {
            PrintLine("INFO. Send data success.");
          } else {
            PrintLine("WARNING. Send data failed.");
          }
          tick_lora = GetTick();
          state_lora = S_MEASUREMENT_INTERVAL;
        } else if (allow_sleep) {
          uint32_t time_to_sleep = LoRaComponGetWaitingTime();

          if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
            DEBUG_PRINTLINE("S_SEND_DATA_WAIT time_to_sleep=%u", time_to_sleep);
            EnterSleep(time_to_sleep, false);
          }
        }
        break;

      default:
        state_lora = S_IDLE;
        break;
    }
    // Check connection
    if ((state_lora != S_IDLE) && (state_lora != S_WAIT_JOIN) && (!LoRaComponIsJoined())) {
      PrintLine("WARNING. Connection lost.");
      state_lora = S_IDLE;
    }
  }
  LoRaComponStop();

  //
  PrintLine("INFO. AppOpTask ended.");
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
  AppDataInit();
  LedInit();
  SensorInit();
  ButtonInit();

  // Create task
  if (xTaskCreate(AppOpTask, "AppOp", 4096, NULL, TASK_PRIO_GENERAL, &gAppOpHandle) != pdPASS) {
    PrintLine("ERROR. Failed to create AppOp task.");
    return -1;
  } else {
    return 0;
  }
}
