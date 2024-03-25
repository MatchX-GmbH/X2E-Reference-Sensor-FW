//==========================================================================
// Test
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
#include "test.h"

#include "app_utils.h"
#include "debug.h"
#include "driver/gpio.h"
#include "led.h"
#include "mx_target.h"
#include "sleep.h"

//==========================================================================
// Defines
//==========================================================================


//==========================================================================
// Constants
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
//==========================================================================
void TestGo(void) {
  //
  if (IsWakeByReset()) {
    PrintLine("Boot from reset.");
  }
  LedInit();

  //
  PrintLine("Hello World");

  //
  bool led_state = true;
  for (;;) {
    // Read input from console
    int key_in = getc(stdin);
    if (key_in >= 0) {
      PrintLine("key pressed %d (0x%X)", key_in, key_in);
    }

    //
    LedSet(led_state);
    led_state = !led_state;

    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}