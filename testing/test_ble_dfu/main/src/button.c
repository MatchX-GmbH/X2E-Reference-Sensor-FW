//==========================================================================
//==========================================================================
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "debug.h"
#include "app_utils.h"
#include "task_priority.h"
#include "mutex_helper.h"
#include "driver/gpio.h"

#include "button.h"

//==========================================================================
// Defines
//==========================================================================
// Task
#define TASK_STACK_SIZE 2048
#define TASK_SLEEP_TIME 50

// GPIO
#define GPIO_BUTTON 0
#define GPIO_NUM_BUTTON (gpio_num_t)(GPIO_BUTTON)

// State machine
enum { S_IDLE = 0, S_BUTTON_GOES_HIGH, S_BUTTON_HIGH_WAIT, S_BUTTON_LOW_WAIT };

//
struct TButtonInfo {
  uint32_t tickStartHold;
  bool isPressedOnce;
  bool isPressed;
};

//==========================================================================
// Variables
//==========================================================================

// Task info
static TaskHandle_t gMyHangle;

//
static struct TButtonInfo gButtonInfo;

//==========================================================================
// Button Task (Polling input at 50ms)
//==========================================================================
static void ButtonTask(void *pvParameters) {
  int state = S_IDLE;
  unsigned long tick = 0;
  DEBUG_PRINTLINE("ButtonTask Start, P=%d", uxTaskPriorityGet (NULL));

  //
  while (1) {
    vTaskDelay(TASK_SLEEP_TIME / portTICK_PERIOD_MS);
    int current_button = 0;
    if (gpio_get_level(GPIO_NUM_BUTTON) == 0) {
      current_button = 1;
    }

    switch (state) {
      case S_IDLE:
        if (current_button) {
          tick = GetTick();
          state = S_BUTTON_GOES_HIGH;
        }
        break;
      case S_BUTTON_GOES_HIGH:
        DEBUG_PRINTLINE("Button pressed.");
        if (TakeMutex()) {
          gButtonInfo.isPressed = true;
          gButtonInfo.isPressedOnce = true;
          gButtonInfo.tickStartHold = GetTick();
          state = S_BUTTON_HIGH_WAIT;
          FreeMutex();
        }
        break;
      case S_BUTTON_HIGH_WAIT:
        if (current_button == 0) {
          tick = GetTick();
          state = S_BUTTON_LOW_WAIT;
        }
        break;
      case S_BUTTON_LOW_WAIT:
        if (current_button) {
          state = S_BUTTON_HIGH_WAIT;
        } else if (TickElapsed(tick) >= 200) {
          DEBUG_PRINTLINE("Button released.");
          if (TakeMutex()) {
            gButtonInfo.isPressed = false;
            state = S_IDLE;
            FreeMutex();
          }
        }
        break;
      default:
        state = S_IDLE;
    }
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t ButtonInit(void) {
  // Setup GPIO
  gpio_reset_pin(GPIO_NUM_BUTTON);
  gpio_config_t io_conf;
  io_conf.intr_type =
      (gpio_int_type_t)(GPIO_PIN_INTR_DISABLE);   // disable interrupt
  io_conf.mode = (gpio_mode_t)(GPIO_MODE_INPUT);  // set as output mode
  io_conf.pin_bit_mask = (1ul << GPIO_BUTTON);    // bit mask of the pins
  io_conf.pull_down_en = (gpio_pulldown_t)(1);    // disable pull-down mode
  io_conf.pull_up_en = (gpio_pullup_t)(0);        // disable pull-up mode
  gpio_config(&io_conf);

  // Init variables
  InitMutex ();
  memset(&gButtonInfo, 0, sizeof(gButtonInfo));
  gMyHangle = NULL;

  //
  xTaskCreate(ButtonTask, "ButtonTask", TASK_STACK_SIZE, NULL, TASK_PRIO_GENERAL,
              &gMyHangle);

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
bool ButtonIsPressedOnce(void) {
  bool ret = false;

  if (TakeMutex()) {
    if (gButtonInfo.isPressedOnce) ret = true;
    FreeMutex();
  }
  return ret;
}

//==========================================================================
// Clear button
//==========================================================================
void ButtonClear(void) {
  if (TakeMutex()) {
    gButtonInfo.isPressedOnce = false;
    FreeMutex();
  }
}

//==========================================================================
// Check Button held
//==========================================================================
bool ButtonIsHeld(uint32_t aTime) {
  bool ret = false;

  if (TakeMutex()) {
    uint32_t tick_start_hold = gButtonInfo.tickStartHold;
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
bool ButtonIsPressed(void) {
  bool ret = false;

  if (TakeMutex()) {
    if (gButtonInfo.isPressed) ret = true;
    FreeMutex();
  }
  return ret;
}
