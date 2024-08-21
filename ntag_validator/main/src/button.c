//==========================================================================
//==========================================================================
#include "button.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "app_utils.h"
#include "debug.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mutex_helper.h"
#include "task_priority.h"

//==========================================================================
// Defines
//==========================================================================
// Task
#define TASK_STACK_SIZE 2048
#define TASK_SLEEP_TIME 50

//
#define NUM_OF_BUTTON 2

// GPIO
#define GPIO_BUTTON0 0
#define GPIO_NUM_BUTTON0 (gpio_num_t)(GPIO_BUTTON0)
#define GPIO_BUTTON1 7
#define GPIO_NUM_BUTTON1 (gpio_num_t)(GPIO_BUTTON1)

// State machine
enum { S_IDLE = 0, S_BUTTON_GOES_HIGH, S_BUTTON_HIGH_WAIT, S_BUTTON_LOW_WAIT };

//
typedef struct {
  const char *name;
  int8_t state;
  uint32_t tick;
  uint32_t tickStartHold;
  bool isPressedOnce;
  bool isPressed;
} ButtonInfo_t;

//==========================================================================
// Variables
//==========================================================================

// Task info
static TaskHandle_t gMyHangle;

//
static ButtonInfo_t gButtonInfo[NUM_OF_BUTTON];

//==========================================================================
// Process Button
//==========================================================================
static void ProcessButton(ButtonInfo_t *aButton, uint8_t aCurrentValue) {
  switch (aButton->state) {
    case S_IDLE:
      if (aCurrentValue) {
        aButton->tick = GetTick();
        aButton->state = S_BUTTON_GOES_HIGH;
      }
      break;
    case S_BUTTON_GOES_HIGH:
      DEBUG_PRINTLINE("Button %s pressed.", aButton->name);
      if (TakeMutex()) {
        aButton->isPressed = true;
        aButton->isPressedOnce = true;
        aButton->tickStartHold = GetTick();
        aButton->state = S_BUTTON_HIGH_WAIT;
        FreeMutex();
      }
      break;
    case S_BUTTON_HIGH_WAIT:
      if (aCurrentValue == 0) {
        aButton->tick = GetTick();
        aButton->state = S_BUTTON_LOW_WAIT;
      }
      break;
    case S_BUTTON_LOW_WAIT:
      if (aCurrentValue) {
        aButton->state = S_BUTTON_HIGH_WAIT;
      } else if (TickElapsed(aButton->tick) >= 200) {
        DEBUG_PRINTLINE("Button %s released.", aButton->name);
        if (TakeMutex()) {
          aButton->isPressed = false;
          aButton->state = S_IDLE;
          FreeMutex();
        }
      }
      break;
    default:
      aButton->state = S_IDLE;
      break;
  }
}

//==========================================================================
// Button Task (Polling input at 50ms)
//==========================================================================
static void ButtonTask(void *pvParameters) {
  DEBUG_PRINTLINE("ButtonTask Start, P=%d", uxTaskPriorityGet(NULL));

  //
  while (1) {
    vTaskDelay(TASK_SLEEP_TIME / portTICK_PERIOD_MS);

    if (gpio_get_level(GPIO_NUM_BUTTON0) == 0) {
      ProcessButton(&gButtonInfo[0], 1);
    } else {
      ProcessButton(&gButtonInfo[0], 0);
    }
    if (gpio_get_level(GPIO_NUM_BUTTON1) == 0) {
      ProcessButton(&gButtonInfo[1], 0);
    } else {
      ProcessButton(&gButtonInfo[1], 1);
    }
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t ButtonInit(void) {
  gpio_config_t io_conf;

  // Setup GPIO
  gpio_reset_pin(GPIO_NUM_BUTTON0);
  io_conf.intr_type = (gpio_int_type_t)(GPIO_PIN_INTR_DISABLE);  // disable interrupt
  io_conf.mode = (gpio_mode_t)(GPIO_MODE_INPUT);                 // set as output mode
  io_conf.pin_bit_mask = (1ul << GPIO_BUTTON0);                  // bit mask of the pins
  io_conf.pull_down_en = (gpio_pulldown_t)(0);                   // disable pull-down mode
  io_conf.pull_up_en = (gpio_pullup_t)(0);                       // enable pull-up mode
  gpio_config(&io_conf);

  gpio_reset_pin(GPIO_NUM_BUTTON1);
  io_conf.intr_type = (gpio_int_type_t)(GPIO_PIN_INTR_DISABLE);  // disable interrupt
  io_conf.mode = (gpio_mode_t)(GPIO_MODE_INPUT);                 // set as output mode
  io_conf.pin_bit_mask = (1ul << GPIO_BUTTON1);                  // bit mask of the pins
  io_conf.pull_down_en = (gpio_pulldown_t)(0);                   // disable pull-down mode
  io_conf.pull_up_en = (gpio_pullup_t)(0);                       // enable pull-up mode
  gpio_config(&io_conf);

  // Init variables
  InitMutex();
  memset(&gButtonInfo, 0, sizeof(gButtonInfo));
  gButtonInfo[0].name = "B0";
  gButtonInfo[1].name = "B1";
  gMyHangle = NULL;

  //
  xTaskCreate(ButtonTask, "ButtonTask", TASK_STACK_SIZE, NULL, TASK_PRIO_GENERAL, &gMyHangle);

  if (gMyHangle == NULL) {
    PrintLine("ERROR. ButtonInit xTaskCreate() failed.");
    return -1;
  }
  DEBUG_PRINTLINE("ButtonTask created.");

  return 0;
}

//==========================================================================
// Check button is pressed once
//==========================================================================
bool ButtonIsPressedOnce(uint8_t aIdx) {
  bool ret = false;

  if (aIdx >= NUM_OF_BUTTON) {
    return ret;
  }

  if (TakeMutex()) {
    if (gButtonInfo[aIdx].isPressedOnce) ret = true;
    FreeMutex();
  }
  return ret;
}

//==========================================================================
// Clear button
//==========================================================================
void ButtonClear(uint8_t aIdx) {
  if (aIdx >= NUM_OF_BUTTON) {
    return;
  }
  if (TakeMutex()) {
    gButtonInfo[aIdx].isPressedOnce = false;
    FreeMutex();
  }
}

//==========================================================================
// Check Button held
//==========================================================================
bool ButtonIsHeld(uint8_t aIdx, uint32_t aTime) {
  bool ret = false;

  if (aIdx >= NUM_OF_BUTTON) {
    return ret;
  }

  if (TakeMutex()) {
    uint32_t tick_start_hold = gButtonInfo[aIdx].tickStartHold;
    FreeMutex();
    if (TickElapsed(tick_start_hold) >= aTime) {
      ret = true;
    }
  }
  return ret;
}

//==========================================================================
// Check Button is pressed
//==========================================================================
bool ButtonIsPressed(uint8_t aIdx) {
  bool ret = false;

  if (aIdx >= NUM_OF_BUTTON) {
    return ret;
  }

  if (TakeMutex()) {
    if (gButtonInfo[aIdx].isPressed) ret = true;
    FreeMutex();
  }
  return ret;
}
