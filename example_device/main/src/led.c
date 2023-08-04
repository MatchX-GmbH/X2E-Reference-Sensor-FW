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
#include "led.h"

#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "driver/gpio.h"
#include "mutex_helper.h"
#include "task_priority.h"

//==========================================================================
// Defines
//==========================================================================
#define GPIO_NUM_LED 21

// Led Info
typedef struct {
  int16_t state;
  uint32_t tick;
  uint32_t onTime;
  uint32_t offTime;
  int16_t cycleCount;
  bool running;
  gpio_num_t gpioNumber;
  int8_t mode;
} LedInfo_t;

// State
enum {
  S_LED_IDLE = 0,
  S_LED_TURN_ON,
  S_LED_ON_WAIT,
  S_LED_TURN_OFF,
  S_LED_OFF_WAIT,
  S_LED_CHECK_REPEAT,
  S_LED_KICK_START,
  S_LED_END,
};

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gLedTaskHandle = NULL;
static LedInfo_t gLedInfo;

//==========================================================================
// LED state machine
//==========================================================================
static void ProcessLed(LedInfo_t *aLed) {
  switch (aLed->state) {
    case S_LED_IDLE:
      if (aLed->cycleCount != 0) {
        // DEBUG_PRINTLINE("LED started.");
        aLed->running = true;
        if ((aLed->onTime == 0) && (aLed->offTime == 0)) {
          aLed->state = S_LED_END;
        } else {
          aLed->state = S_LED_TURN_ON;
        }
      } else if (aLed->running) {
        aLed->state = S_LED_END;
      }
      break;
    case S_LED_TURN_ON:
      if (aLed->onTime > 0) {
        // DEBUG_PRINTLINE("Turn on LED");
        gpio_set_level(aLed->gpioNumber, 0);
        aLed->tick = GetTick();
        aLed->state = S_LED_ON_WAIT;
      } else {
        aLed->state = S_LED_TURN_OFF;
      }
      break;
    case S_LED_ON_WAIT:
      if (TickElapsed(aLed->tick) >= aLed->onTime) {
        aLed->state = S_LED_TURN_OFF;
      }
      break;
    case S_LED_TURN_OFF:
      if (aLed->offTime > 0) {
        // DEBUG_PRINTLINE("Turn off LED");
        gpio_set_level(aLed->gpioNumber, 1);
        aLed->tick = GetTick();
        aLed->state = S_LED_OFF_WAIT;
      } else {
        aLed->state = S_LED_CHECK_REPEAT;
      }
      break;
    case S_LED_OFF_WAIT:
      if (TickElapsed(aLed->tick) >= aLed->offTime) {
        aLed->state = S_LED_CHECK_REPEAT;
      }
      break;
    case S_LED_CHECK_REPEAT:
      if (aLed->cycleCount > 0) {
        aLed->cycleCount--;
      } else if (aLed->cycleCount == 0) {
        aLed->state = S_LED_END;
      } else {
        aLed->state = S_LED_TURN_ON;
      }
      break;
    case S_LED_KICK_START:
      // Turn off LED
      gpio_set_level(aLed->gpioNumber, 1);
      aLed->state = S_LED_IDLE;
      break;
    case S_LED_END:
    default:
      // Turn off LED
      gpio_set_level(aLed->gpioNumber, 1);
      aLed->cycleCount = 0;
      aLed->state = S_LED_IDLE;
      aLed->running = false;
      // DEBUG_PRINTLINE("LED stopped.");
      break;
  }
}

//==========================================================================
// LED Task
//==========================================================================
static void LedTask(void *pvParameters) {
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    bool led_running = false;
    if (TakeMutex()) {
      ProcessLed(&gLedInfo);
      led_running = gLedInfo.running;
      FreeMutex();
    }

    // When no led running, blocked here, wait for notification
    if (!led_running) {
      // DEBUG_PRINTLINE("LED ended.");
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      // DEBUG_PRINTLINE("LED event.");
    }
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t LedInit(void) {
  // Setup GPIO
  gpio_reset_pin(GPIO_NUM_LED);
  gpio_set_direction(GPIO_NUM_LED, GPIO_MODE_OUTPUT);

  //
  InitMutex();
  memset(&gLedInfo, 0, sizeof(gLedInfo));
  gLedInfo.gpioNumber = GPIO_NUM_LED;

  // Create task
  if (xTaskCreate(LedTask, "Led", 4096, NULL, TASK_PRIO_GENERAL, &gLedTaskHandle) != pdPASS) {
    printf("ERROR. Failed to create LED task.\n");
    return -1;
  }

  LedSet(LED_MODE_OFF, 0);
  return 0;
}

//==========================================================================
// Set LED
//==========================================================================
void LedSet(int8_t aMode, int16_t aCycleCount) {
  uint32_t on_time = 0;
  uint32_t off_time = 0;

  switch (aMode) {
    case LED_MODE_ON:
      on_time = 500;
      off_time = 0;
      break;
    case LED_MODE_FAST_BLINKING:
      on_time = 100;
      off_time = 100;
      break;
    case LED_MODE_SLOW_BLINKING:
      on_time = 300;
      off_time = 1200;
      break;
    default:
      // included LED_MODE_ALWAYS_OFF
      break;
  }

  if (TakeMutex()) {
    if (gLedInfo.mode != aMode) {
      gLedInfo.mode = aMode;
      gLedInfo.onTime = on_time;
      gLedInfo.offTime = off_time;
      gLedInfo.cycleCount = aCycleCount;
      gLedInfo.state = S_LED_KICK_START;
      gLedInfo.running = true;
    }
    FreeMutex();
  }
  if (gLedTaskHandle != NULL) {
    xTaskNotifyGive(gLedTaskHandle);
  }
}
