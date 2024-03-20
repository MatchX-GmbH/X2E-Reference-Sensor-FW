//==========================================================================
// Battery Management chip
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
#include "sleep.h"

#include "app_utils.h"
#include "debug.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

//==========================================================================
// Variables
//==========================================================================
// Store at RTC area, will keep while sleep
static RTC_DATA_ATTR uint32_t gTickEnterSleep;

//==========================================================================
// Set wakeup event
//==========================================================================
static void SetWakeupEvent(uint32_t aTimeToSleep, bool aWakeByButton) {
  if (aTimeToSleep > 0) {
    DEBUG_PRINTLINE("Enabling timer wakeup, %dms", aTimeToSleep);
    esp_sleep_enable_timer_wakeup((uint64_t)aTimeToSleep * 1000);
  }

  if (aWakeByButton) {
    const int ext_wakeup_pin_1 = 0;
    const uint64_t ext_wakeup_pin_1_mask = (1ULL << ext_wakeup_pin_1);

    DEBUG_PRINTLINE("Enabling EXT1 wakeup on pins GPIO%d", ext_wakeup_pin_1);
    esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask, ESP_EXT1_WAKEUP_ALL_LOW);
  }

  // // Isolate GPIO26 pin from external circuits.
  // rtc_gpio_isolate(GPIO_NUM_26);
}

//==========================================================================
// Enter deep sleep
//==========================================================================
void EnterDeepSleep(uint32_t aTimeToSleep, bool aWakeByButton) {
  //
  SetWakeupEvent(aTimeToSleep, aWakeByButton);

  //
  gTickEnterSleep = GetTick();
  DEBUG_PRINTLINE("DeepSleep gTickEnterSleep=%u.", gTickEnterSleep);
  esp_deep_sleep_start();

  // Failed
  PrintLine("Failed to enter deep sleep.");
}

//==========================================================================
// Enter light sleep
//==========================================================================
void EnterLightSleep(uint32_t aTimeToSleep, bool aWakeByButton) {
  //
  SetWakeupEvent(aTimeToSleep, aWakeByButton);

  //
  gTickEnterSleep = GetTick();
  DEBUG_PRINTLINE("LightSleep gTickEnterSleep=%u.", gTickEnterSleep);
  if (DEBUG) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  esp_light_sleep_start();

  //
  DEBUG_PRINTLINE("Wakeup from light sleep.");
}

//==========================================================================
// Is wake up by reset
//==========================================================================
bool IsWakeByReset(void) {
  bool ret = false;
  DEBUG_PRINTLINE("Wakeup gTickEnterSleep=%u", gTickEnterSleep);
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT1: {
      uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_pin_mask != 0) {
        int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
        DEBUG_PRINTLINE("Wake up from GPIO %d", pin);
      } else {
        DEBUG_PRINTLINE("Wake up from GPIO");
      }
      DEBUG_PRINTLINE("Time spent in sleep: %ldms", TickElapsed(gTickEnterSleep));
      break;
    }
    case ESP_SLEEP_WAKEUP_TIMER: {
      DEBUG_PRINTLINE("Wake up from timer.");
      DEBUG_PRINTLINE("Time spent in sleep: %ldms", TickElapsed(gTickEnterSleep));
      break;
    }
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      DEBUG_PRINTLINE("Not wake up from event.");
      ret = true;
      break;
  }
  return ret;
}