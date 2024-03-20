//==========================================================================
// Test pin
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
#ifndef INC_TEST_PIN_H
#define INC_TEST_PIN_H

//==========================================================================
//==========================================================================
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "driver/gpio.h"

//==========================================================================
//==========================================================================
#define TestPinInit()                                                          \
  { gpio_reset_pin(GPIO_NUM_10); \
    gpio_set_direction(GPIO_NUM_10, GPIO_MODE_OUTPUT); \
    gpio_reset_pin(GPIO_NUM_11); \
    gpio_set_direction(GPIO_NUM_11, GPIO_MODE_OUTPUT); \
  }

#define TestPinDeInit()                                                        \
  { gpio_reset_pin(GPIO_NUM_10); \
    gpio_reset_pin(GPIO_NUM_11); \
  }

#define TestPin0High()    { gpio_set_level(GPIO_NUM_10, 1); }
#define TestPin0Low()     { gpio_set_level(GPIO_NUM_10, 0); }

#define TestPin1High()    { gpio_set_level(GPIO_NUM_11, 1); }
#define TestPin1Low()     { gpio_set_level(GPIO_NUM_11, 0); }

//==========================================================================
//==========================================================================
#endif // INC_TEST_PIN_H