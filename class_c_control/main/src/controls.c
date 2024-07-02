//==========================================================================
// Controls
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
#include "controls.h"

#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "driver/gpio.h"
#include "mutex_helper.h"
#include "task_priority.h"

//==========================================================================
// Defines
//==========================================================================
#define GPIO_NUM_CTRL_0 14

// Controls Info
typedef struct {
  int16_t state;
  uint32_t tick;
  uint32_t onTime;
  uint32_t offTime;
  int16_t cycleCount;
  bool running;
  gpio_num_t gpioNumber;
  int8_t mode;
} CtrlInfo_t;

// State
enum {
  S_CTRL_IDLE = 0,
  S_CTRL_TURN_ON,
  S_CTRL_ON_WAIT,
  S_CTRL_TURN_OFF,
  S_CTRL_OFF_WAIT,
  S_CTRL_CHECK_REPEAT,
  S_CTRL_KICK_START,
  S_CTRL_END,
};

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gCtrlTaskHandle = NULL;
static CtrlInfo_t gCtrlInfo;

//==========================================================================
// Controls state machine
//==========================================================================
static void ProcessControls(CtrlInfo_t *aCtrl) {
  switch (aCtrl->state) {
    case S_CTRL_IDLE:
      if (aCtrl->cycleCount != 0) {
        // DEBUG_PRINTLINE("CTRL started.");
        aCtrl->running = true;
        if ((aCtrl->onTime == 0) && (aCtrl->offTime == 0)) {
          aCtrl->state = S_CTRL_END;
        } else {
          aCtrl->state = S_CTRL_TURN_ON;
        }
      } else if (aCtrl->running) {
        aCtrl->state = S_CTRL_END;
      }
      break;
    case S_CTRL_TURN_ON:
      if (aCtrl->onTime > 0) {
        // DEBUG_PRINTLINE("Turn on CTRL");
        gpio_set_level(aCtrl->gpioNumber, 1);
        aCtrl->tick = GetTick();
        aCtrl->state = S_CTRL_ON_WAIT;
      } else {
        aCtrl->state = S_CTRL_TURN_OFF;
      }
      break;
    case S_CTRL_ON_WAIT:
      if (TickElapsed(aCtrl->tick) >= aCtrl->onTime) {
        aCtrl->state = S_CTRL_TURN_OFF;
      }
      break;
    case S_CTRL_TURN_OFF:
      if (aCtrl->offTime > 0) {
        // DEBUG_PRINTLINE("Turn off CTRL");
        gpio_set_level(aCtrl->gpioNumber, 0);
        aCtrl->tick = GetTick();
        aCtrl->state = S_CTRL_OFF_WAIT;
      } else {
        aCtrl->state = S_CTRL_CHECK_REPEAT;
      }
      break;
    case S_CTRL_OFF_WAIT:
      if (TickElapsed(aCtrl->tick) >= aCtrl->offTime) {
        aCtrl->state = S_CTRL_CHECK_REPEAT;
      }
      break;
    case S_CTRL_CHECK_REPEAT:
      if (aCtrl->cycleCount > 0) {
        aCtrl->cycleCount--;
      } else if (aCtrl->cycleCount == 0) {
        aCtrl->state = S_CTRL_END;
      } else {
        aCtrl->state = S_CTRL_TURN_ON;
      }
      break;
    case S_CTRL_KICK_START:
      // Turn off CTRL
      gpio_set_level(aCtrl->gpioNumber, 0);
      aCtrl->state = S_CTRL_IDLE;
      break;
    case S_CTRL_END:
    default:
      // Turn off CTRL
      gpio_set_level(aCtrl->gpioNumber, 0);
      aCtrl->cycleCount = 0;
      aCtrl->state = S_CTRL_IDLE;
      aCtrl->running = false;
      // DEBUG_PRINTLINE("CTRL stopped.");
      break;
  }
}

//==========================================================================
// Controls Task
//==========================================================================
static void ControlsTask(void *pvParameters) {
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    bool ctrl_running = false;
    if (TakeMutex()) {
      ProcessControls(&gCtrlInfo);
      ctrl_running = gCtrlInfo.running;
      FreeMutex();
    }

    // When not running, blocked here, wait for notification
    if (!ctrl_running) {
      // DEBUG_PRINTLINE("CTRL ended.");
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      // DEBUG_PRINTLINE("CTRL event.");
    }
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t ControlsInit(void) {
  // Setup GPIO
  gpio_reset_pin(GPIO_NUM_CTRL_0);
  gpio_set_direction(GPIO_NUM_CTRL_0, GPIO_MODE_OUTPUT);

  //
  InitMutex();
  memset(&gCtrlInfo, 0, sizeof(gCtrlInfo));
  gCtrlInfo.gpioNumber = GPIO_NUM_CTRL_0;

  // Create task
  if (xTaskCreate(ControlsTask, "Controls", 4096, NULL, TASK_PRIO_GENERAL, &gCtrlTaskHandle) != pdPASS) {
    printf("ERROR. Failed to create Controls task.\n");
    return -1;
  }

  ControlsSet(CTRL_MODE_OFF, 0);
  return 0;
}

//==========================================================================
// Set Controls
//==========================================================================
void ControlsSet(int8_t aMode, int16_t aCycleCount) {
  uint32_t on_time = 0;
  uint32_t off_time = 0;

  switch (aMode) {
    case CTRL_MODE_ON:
      on_time = 500;
      off_time = 0;
      break;
    case CTRL_MODE_BLINKING:
      on_time = 200;
      off_time = 200;
      break;
    default:
      // included CTRL_MODE_ALWAYS_OFF
      break;
  }

  if (TakeMutex()) {
    if (gCtrlInfo.mode != aMode) {
      gCtrlInfo.mode = aMode;
      gCtrlInfo.onTime = on_time;
      gCtrlInfo.offTime = off_time;
      gCtrlInfo.cycleCount = aCycleCount;
      gCtrlInfo.state = S_CTRL_KICK_START;
      gCtrlInfo.running = true;
    }
    FreeMutex();
  }
  if (gCtrlTaskHandle != NULL) {
    xTaskNotifyGive(gCtrlTaskHandle);
  }
}
