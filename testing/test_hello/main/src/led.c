//==========================================================================
// LED
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

#include "driver/gpio.h"

//==========================================================================
// Defines
//==========================================================================
#define GPIO_NUM_LED 21

//==========================================================================
// Init
//==========================================================================
int8_t LedInit(void) {
  // Setup GPIO
  gpio_reset_pin(GPIO_NUM_LED);
  gpio_set_direction(GPIO_NUM_LED, GPIO_MODE_OUTPUT);

  LedSet(false);
  return 0;
}

//==========================================================================
// Set LED
//==========================================================================
void LedSet(bool aOn) {
  if (aOn) {
    gpio_set_level(GPIO_NUM_LED, 0);
  }
  else {
    gpio_set_level(GPIO_NUM_LED, 1);
  }
}
