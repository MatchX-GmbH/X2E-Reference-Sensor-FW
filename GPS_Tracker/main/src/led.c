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

#include "driver/gpio.h"

//==========================================================================
// Defines
//==========================================================================
#define GPIO_NUM_LED GPIO_NUM_21

//==========================================================================
// Init
//==========================================================================
void LedInit(void) {
  gpio_reset_pin(GPIO_NUM_LED);
  gpio_set_direction(GPIO_NUM_LED, GPIO_MODE_OUTPUT);
  LedSet(false);
}

//==========================================================================
// Set LED on/off
//==========================================================================
void LedSet(bool aOn) {
  if (aOn) {
    gpio_set_level(GPIO_NUM_LED, 0);

  } else {
    gpio_set_level(GPIO_NUM_LED, 1);
  }
}
