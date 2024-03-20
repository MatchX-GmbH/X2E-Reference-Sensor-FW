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
#include "ble_dfu.h"
#include "button.h"
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

  //
  LedInit();
  ButtonInit();
  BleDfuServerInit();

  //
  for (;;) {
    int key_in = 0;

    //
    PrintLine("====================================");
    PrintLine("test_ble_dfu");
    PrintLine("====================================");
    PrintLine("  1) Start BLE DFU Server");
    PrintLine("  2) Stop BLE DFU Server");
    PrintLine("  or hold button 5s to start BLE DFU Server");

    // Wait key or button
    for (;;) {
      if ((!BleDfuServerIsStarted()) && (ButtonIsPressed())) {
        if (ButtonIsHeld(5000)) {
          key_in = 'a';
          break;
        }
      } else {
        ButtonClear();
      }
      key_in = getc(stdin);
      if (key_in >= 0) {
        break;
      }

      //
      if (BleDfuServerIsConnected()) {
        LedSet(true);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        LedSet(false);
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
      if (BleDfuServerIsStarted()) {
        LedSet(true);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        LedSet(false);
        vTaskDelay(200 / portTICK_PERIOD_MS);
      } else {
        LedSet(true);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        LedSet(false);
        vTaskDelay(950 / portTICK_PERIOD_MS);
      }
    }

    //
    switch (key_in) {
      case '1':
        if (!BleDfuServerIsStarted()) {
          BleDfuServerStart("X2E-DFU");
        }
        break;
      case '2':
        BleDfuServerStop();
        break;
      case 'a':
        if (!BleDfuServerIsStarted()) {
          BleDfuServerStart("X2E-DFU");
        }
        break;
      default:
        DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
        break;
    }

    //
  }
}