//==========================================================================
// Testing platform implementation of LoRa
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
#include "test_platform.h"

#include <stddef.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "mx_target.h"
#include "platform/board.h"
#include "platform/timer.h"
#include "test_pin.h"

//==========================================================================
// Variables
//==========================================================================
static TimerEvent_t gTestingTimer;

//==========================================================================
// Callback for testing timer
//==========================================================================
static uint32_t gTestingValue;
static void TimerCallbackFunc(void *context) {
  DEBUG_PRINTLINE("%s", __func__);
  TestPin0Low();
  gTestingValue ++;
}

//==========================================================================
// Test platform functions
//==========================================================================
void TestPlatform(void) {
  PrintLine("TestPlatform start.");

  // Test UID
  uint8_t uid[8];
  LoRaBoardGetUniqueId(uid);
  Hex2String("UID: ", uid, sizeof(uid));

  // Test timer
  gTestingValue = 0;

  TimerInit(&gTestingTimer, TimerCallbackFunc);
  TimerSetValue(&gTestingTimer, 1000);
  TestPin0High();
  TimerStart(&gTestingTimer);
  vTaskDelay(1200 / portTICK_PERIOD_MS);
  TestPin0Low();

  if (gTestingValue == 1) {
    PrintLine("Timer is OK.");
  } else {
    PrintLine("Timer failed.");
  }

  //
  PrintLine("TestPlatform ended.");
}
