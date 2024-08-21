//==========================================================================
// Test NFC
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
#include "test_nfc.h"

#include "Board_X2E.h"
#include "app_utils.h"
#include "debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ntag.h"
#include "phDriver.h"
#include "phNfcLib.h"
#include "phbalReg.h"
#include "task_priority.h"

//==========================================================================
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
// Constants
//==========================================================================
static const uint8_t kDemoKey[NUM_OF_NTAG_KEY][NTAG_KEY_SIZE] = {
    {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
};

static const uint8_t kDemoVerifyData[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

//==========================================================================
// Flush keys
//==========================================================================
static void FlushInput(void) {
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    int key_in = getc(stdin);
    if (key_in < 0) break;
  }
}

//==========================================================================
// Set keys
//==========================================================================
static int8_t SetupTest(void) {
  int8_t ret = 0;
  if (NtagInit() < 0) {
    ret = -1;
  } else if (NtagSetVerifyData(kDemoVerifyData, sizeof(kDemoVerifyData)) < 0) {
    ret = -1;
  } else {
    for (uint8_t i = 0; i < NUM_OF_NTAG_KEY; i++) {
      if (NtagSetKey(i, kDemoKey[i], NTAG_KEY_SIZE) < 0) {
        ret = -1;
        break;
      }
    }
  }
  return ret;
}

//==========================================================================
// Initialize a NTAG
//==========================================================================
void TestNfc_Initialize(void) {
  PrintLine("Test NFC - Initialize");

  if (SetupTest() >= 0) {
    PrintLine("Waiting a blank TAG.");
    NtagWait(NTAG_ACTION_INITIALIZE, 30000);
  }
  PrintLine("Test NFC ended");
}

//==========================================================================
// Validate a NTAG
//==========================================================================
void TestNfc_Validate(void) {
  PrintLine("Test NFC - Validate");

  if (SetupTest() >= 0) {
    PrintLine("Waiting NTAG.");
    NtagWait(NTAG_ACTION_VALIDATE, 30000);
    NtagActionResult_t *result = NtagGetResult();
    if (result->actionSuccess) {
      if (result->dataVerified) {
        PrintLine("Data Verified.");
      } else {
        PrintLine("Data verify failed.");
      }
      if (result->tamperClosed) {
        PrintLine("Tamper closed.");
      } else {
        PrintLine("Tamper opened.");
      }
      if (result->validated) {
        PrintLine("NTAG validation success.");
      } else {
        PrintLine("NTAG validation fail.");
      }
    }
  }
  PrintLine("Test NFC ended");
}

//==========================================================================
// Reset a NTAG
//==========================================================================
void TestNfc_Reset(void) {
  PrintLine("Test NFC - Reset");

  if (SetupTest() >= 0) {
    PrintLine("Waiting NTAG.");
    NtagWait(NTAG_ACTION_RESET, 30000);
  }
  PrintLine("Test NFC ended");
}