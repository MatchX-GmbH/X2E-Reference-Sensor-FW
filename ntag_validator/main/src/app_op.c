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
//         typedef enum : tailing _e
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
#include "display.h"
#include "driver/gpio.h"
#include "led.h"
#include "logo_matchx.h"
#include "lora_compon.h"
#include "matchx_payload.h"
#include "ntag.h"
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
  S_WAIT_TAG,
  S_WAIT_PROVISIONING,
  S_WAIT_JOIN,
  S_JOINED,
  S_MEASUREMENT_START,
  S_MEASUREMENT_WAIT,
  S_MEASUREMENT_INTERVAL,
  S_SEND_DATA,
  S_SEND_DATA_WAIT,
  S_WAIT_INTERVAL,
  S_VALIDATE_NTAG,
} LoRaState_e;

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
static const char *kPidHashSuffix = ".MatchX";

// Fixed DEMO keys for NTAG
static const uint8_t kDemoKey[NUM_OF_NTAG_KEY][NTAG_KEY_SIZE] = {
    {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
};

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gAppOpHandle = NULL;
static bool gNtagReady = false;

// PID
static char gPid[32];

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
  NtagActionResult_t *ntag_result;

  uint8_t buf[32];
  uint8_t *ptr;

  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);
  SensorGetResult(&result);

  ntag_result = NtagGetResult();

  //
  if (batt_current >= VALUE_EXT_POWER_CURRENT_THRESHOLD) {
    // Set reporting battery status to external power.
    LoRaComponSetExtPower();
  } else {
    LoRaComponSetBatteryPercent(batt_percentage);
  }

  ptr = buf;
  if (gNtagReady) {
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 2));
    ptr += PackU8(ptr, MX_SENSOR_DIGITAL);
    ptr += PackU8(ptr, ntag_result->validated ? 1 : 0);
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 2));
    ptr += PackU8(ptr, MX_SENSOR_DIGITAL);
    ptr += PackU8(ptr, ntag_result->dataVerified ? 1 : 0);
    ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 2));
    ptr += PackU8(ptr, MX_SENSOR_DIGITAL);
    ptr += PackU8(ptr, ntag_result->tamperClosed ? 1 : 0);
  }

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
  DisplayPrepareForSleep();
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
  DisplayResumeFromSleep();
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
      LoRaComponStart2(gPid, pid_hash, aWakeFromSleep, true);
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
// Displays
//==========================================================================
static void ScreenStartup(void) {
  DisplayClearScreen();
  DisplayDrawGrayArray(0, 8, LOGO_WIDTH, LOGO_HEIGHT, kLogoData);
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(52, 8, "MatchX");
  DisplayDrawString(52, 20, "NTAG");
  DisplayDrawString(52, 32, "Validator");
}

static void ScreenJoining(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Joining...");
}

static void ScreenJoined(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Joined to");
  DisplayDrawString(4, 18, "LoRaWAN.");
}

static void ScreenMeasuring(void) {
  char buf[20];

  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Measuring and");
  DisplayDrawString(4, 18, "checking NTAG");
  DisplayDrawString(4, 32, "...");

  float batt_percentage, batt_current, batt_voltage;
  DisplaySetFont(DISPLAY_FONT_6x8);
  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);
  if (IsExtPower()) {
    snprintf(buf, sizeof(buf), "%.1fV", batt_voltage);
    DisplayDrawString(100, 46, buf);
    DisplayDrawString(100, 55, "[DC]");
  } else {
    if (!isnan(batt_percentage)) {
      snprintf(buf, sizeof(buf), "%.1fV", batt_voltage);
      DisplayDrawString(100, 46, buf);
      snprintf(buf, sizeof(buf), "%3d%%", (int32_t)(batt_percentage + 0.5));
      DisplayDrawString(100, 55, buf);
    }
  }
}

static void DrawResult(void) {
  SensorResult_t sensor_result;
  NtagActionResult_t *ntag_result;
  char buf[20];

  DisplaySetFont(DISPLAY_FONT_6x8);
  ntag_result = NtagGetResult();
  if (ntag_result->actionSuccess) {
    if (ntag_result->validated) {
      DisplayDrawString(4, 20, "NTAG: Good");
    } else {
      DisplayDrawString(4, 20, "NTAG: Invalid");
    }
  } else {
    DisplayDrawString(4, 20, "NTAG: Absent");
  }

  SensorGetResult(&sensor_result);
  if (!isnan(sensor_result.temperature)) {
    snprintf(buf, sizeof(buf), "T: %.1f C", sensor_result.temperature);
    DisplayDrawString(4, 30, buf);
  }
  if (!isnan(sensor_result.humidity)) {
    snprintf(buf, sizeof(buf), "H: %.1f %%RH", sensor_result.humidity);
    DisplayDrawString(4, 40, buf);
  }
  if (!isnan(sensor_result.pressure)) {
    snprintf(buf, sizeof(buf), "P: %d hPa", (int32_t)(sensor_result.pressure + 0.5));
    DisplayDrawString(4, 50, buf);
  }

  float batt_percentage, batt_current, batt_voltage;
  DisplaySetFont(DISPLAY_FONT_6x8);
  SensorGetBattery(&batt_percentage, &batt_current, &batt_voltage);
  if (IsExtPower()) {
    snprintf(buf, sizeof(buf), "%.1fV", batt_voltage);
    DisplayDrawString(100, 46, buf);
    DisplayDrawString(100, 55, "[DC]");
  } else {
    if (!isnan(batt_percentage)) {
      snprintf(buf, sizeof(buf), "%.1fV", batt_voltage);
      DisplayDrawString(100, 46, buf);
      snprintf(buf, sizeof(buf), "%3d%%", (int32_t)(batt_percentage + 0.5));
      DisplayDrawString(100, 55, buf);
    }
  }
}

static void ScreenSending(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Sending data...");

  DrawResult();
}

static void ScreenTxSuccess(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Data sent.");

  DrawResult();
}

static void ScreenTxFail(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Fail to send");
  DisplayDrawString(4, 18, "  data.");
}

static void ScreenWaitTag(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Place to the");
  DisplayDrawString(4, 16, "blank TAG and");
  DisplayDrawString(4, 28, "press button to");
  DisplayDrawString(4, 40, "start the");
  DisplayDrawString(4, 52, "provisioning.");
}

static void ScreenCheckingTag(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Checking TAG...");
}

static void ScreenWaitProvisioning(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Provisioning in");
  DisplayDrawString(4, 16, "progress...");
  DisplayDrawString(4, 30, "Pls hold with");
  DisplayDrawString(4, 42, "the TAG.");
}

static void ScreenInitingTag(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "Initializing");
  DisplayDrawString(4, 16, "TAG...");
}

static void ScreenDfuStarted(void) {
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(4, 4, "DFU started.");
}

//==========================================================================
//==========================================================================
static void AppOpTask(void *param) {
  bool wake_from_sleep;
  uint32_t tick_keep_awake = 0;
  uint32_t time_to_awake = 0;

  PrintLine("INFO. Environmental Sensor (%s).", LoRaComponRegionName());

  // Show startup display
  ScreenStartup();

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
  uint32_t tick_measurement;
  uint32_t tick_dfu = 0;
  LoRaState_e state_lora = S_IDLE;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Button and BLE DFU
    if (ButtonIsPressed(0)) {
      if (ButtonIsHeld(0, 5000)) {
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
            ScreenDfuStarted();
          }
        }
        // Wait button release
        LedSet(LED_MODE_FAST_BLINKING, -1);
        for (;;) {
          if (!ButtonIsPressed(0)) break;
          vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        ButtonClear(0);
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
    if ((!MATCHX_DISABlE_SLEEP) && (!ext_power) && (!BleDfuServerIsStarted()) && (!ButtonIsPressed(0))) {
      allow_sleep = true;
    }

    //
    switch (state_lora) {
      case S_IDLE:
        if (LoRaComponIsProvisioned()) {
          PrintLine("INFO. Device is provisioned.");
          state_lora = S_WAIT_JOIN;
        } else {
          LedSet(LED_MODE_SHORT_PULSE, -1);
          PrintLine("INFO. Place to a blank TAG and press the button to continue.");
          ScreenWaitTag();
          state_lora = S_WAIT_TAG;
        }
        break;
      case S_WAIT_TAG:
        if (ButtonIsPressedOnce(1)) {
          bool found_tag = false;
          ScreenCheckingTag();
          for (int ntag_retry = 0; ntag_retry < 3; ntag_retry++) {
            NtagWait(NTAG_ACTION_CHK_BLANK, 3000);
            NtagActionResult_t *result = NtagGetResult();
            if (result->actionSuccess) {
              found_tag = true;
              break;
            }
          }
          if (found_tag) {
            PrintLine("INFO. Start device provisioning.");
            ScreenWaitProvisioning();
            LoRaComponProceedProvisioning();
            state_lora = S_WAIT_PROVISIONING;
          } else {
            ScreenWaitTag();
          }
          ButtonClear(1);
        }
        break;
      case S_WAIT_PROVISIONING:
        if (LoRaComponIsProvisioned()) {
          ScreenInitingTag();
          if (NtagSetVerifyData((const uint8_t *)gPid, strlen(gPid)) >= 0) {
            for (int ntag_retry = 0; ntag_retry < 3; ntag_retry++) {
              NtagWait(NTAG_ACTION_INITIALIZE, 3000);
              NtagActionResult_t *result = NtagGetResult();
              if (result->actionSuccess) {
                state_lora = S_WAIT_JOIN;
                break;
              }
            }
          }
        }
        break;
      case S_WAIT_JOIN:
        if (LoRaComponIsJoined()) {
          LedSet(LED_MODE_ON, -1);
          ScreenJoined();
          if (LoRaComponIsIsm2400()) {
            PrintLine("INFO. Joined with ISM2400");
          } else {
            PrintLine("INFO. Joined with sub-GHz");
          }
          PrintLine("INFO. Data sending interval, DC=%ds, BATT=%ds", INTERVAL_SENDING_DATA_DC / 1000,
                    INTERVAL_SENDING_DATA_BATT / 1000);
          state_lora = S_MEASUREMENT_START;
        } else {
          LedSet(LED_MODE_SLOW_BLINKING, -1);
          ScreenJoining();
          // if (allow_sleep) {
          //   uint32_t time_to_sleep = LoRaComponGetWaitingTime();
          //   if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
          //     DEBUG_PRINTLINE("S_IDLE time_to_sleep=%u", time_to_sleep);
          //     if ((MATCHX_ENABLE_DEEP_SLEEP) && (time_to_sleep >= TIME_DEEP_SLEEP_THRESHOLD)) {
          //       EnterSleep(time_to_sleep, true);
          //     } else {
          //       EnterSleep(time_to_sleep, false);
          //     }
          //   }
          // }
        }
        break;
      case S_JOINED:
        state_lora = S_MEASUREMENT_START;
        break;
      case S_MEASUREMENT_START:
        ScreenMeasuring();
        tick_measurement = GetTick();
        SensorStartMeas(ext_power);
        state_lora = S_VALIDATE_NTAG;
        break;

      case S_VALIDATE_NTAG:
        if (gNtagReady) {
          if (NtagSetVerifyData((const uint8_t *)gPid, strlen(gPid)) >= 0) {
            for (int ntag_retry = 0; ntag_retry < 3; ntag_retry++) {
              NtagWait(NTAG_ACTION_VALIDATE, 3000);
              NtagActionResult_t *result = NtagGetResult();
              if (result->validated) {
                break;
              }
            }
          }
        }
        state_lora = S_MEASUREMENT_WAIT;
        break;

      case S_MEASUREMENT_WAIT:
        if (SensorIsReady()) {
          state_lora = S_SEND_DATA;
        }
        break;

      case S_MEASUREMENT_INTERVAL:
        LedSet(LED_MODE_ON, -1);
        if (LoRaComponIsRxReady()) {
          GetData();
        }
        if (ButtonIsPressedOnce(1)) {
          ButtonClear(1);
          state_lora = S_MEASUREMENT_START;
        } else if (TickElapsed(tick_measurement) >= INTERVAL_SENDING_DATA_DC) {
          state_lora = S_MEASUREMENT_START;
        } else if ((time_to_awake == 0) || (TickElapsed(tick_keep_awake) >= time_to_awake)) {
          // Determine sleep time
          uint32_t time_to_sleep = INTERVAL_SENDING_DATA_DC;
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
        ScreenSending();
        SendData();
        state_lora = S_SEND_DATA_WAIT;
        break;

      case S_SEND_DATA_WAIT:
        if (LoRaComponIsSendDone()) {
          if (LoRaComponIsSendSuccess()) {
            ScreenTxSuccess();
            PrintLine("INFO. Send data success.");
          } else {
            ScreenTxFail();
            PrintLine("WARNING. Send data failed.");
          }
          ButtonClear(1);
          tick_keep_awake = GetTick();
          time_to_awake = 3000;
          state_lora = S_MEASUREMENT_INTERVAL;
        }
        // } else if (allow_sleep) {
        //   uint32_t time_to_sleep = LoRaComponGetWaitingTime();

        //   if (time_to_sleep > TIME_GO_TO_SLEEP_THRESHOLD) {
        //     DEBUG_PRINTLINE("S_SEND_DATA_WAIT time_to_sleep=%u", time_to_sleep);
        //     EnterSleep(time_to_sleep, false);
        //   }
        // }
        break;

      default:
        state_lora = S_IDLE;
        break;
    }
    // Check connection
    if ((state_lora >= S_JOINED) && (!LoRaComponIsJoined())) {
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
  DisplayInit();

  if (NtagInit() < 0) {
    PrintLine("ERROR. NtagInit() failed.");
  } else {
    gNtagReady = true;
    for (uint8_t i = 0; i < NUM_OF_NTAG_KEY; i++) {
      if (NtagSetKey(i, kDemoKey[i], NTAG_KEY_SIZE) < 0) {
        gNtagReady = false;
        break;
      }
    }
  }

  // Create task
  if (xTaskCreate(AppOpTask, "AppOp", 4096, NULL, TASK_PRIO_GENERAL, &gAppOpHandle) != pdPASS) {
    PrintLine("ERROR. Failed to create AppOp task.");
    return -1;
  } else {
    return 0;
  }
}
