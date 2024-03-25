//==========================================================================
//
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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "lora_compon.h"
#include "mx_target.h"
#include "test_compon.h"
#include "test_platform.h"
#include "test_radio.h"
#include "test_pin.h"

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
//==========================================================================
static void ShowMenu(void) {
  PrintLine("================================================");
  PrintLine("Menu (target=%s)", MxTargetName());
  PrintLine("================================================");
  PrintLine("1) Test LoRa platform funtions.");
  PrintLine("2) SX1261 CW test.");
  PrintLine("3) SX1261 TX test.");
  PrintLine("4) SX1261 RX test.");
  PrintLine("5) SX1280 CW test.");
  PrintLine("6) SX1280 TX test.");
  PrintLine("7) SX1280 RX test.");
  PrintLine("8) Test the LoRa component.");
  PrintLine("================================================");
}

//==========================================================================
// Init
//==========================================================================
void TestInit(void) {
  MxTargetInit();

  // LoRa hardware related init
  LoRaComponHwInit();

  //
  TestPinInit();
  TestPin0Low();
  TestPin1Low();
}

//==========================================================================
//==========================================================================
void TestGo(void) {
  for (;;) {
    ShowMenu();

    // Wait input from debug port
    int key_in = -1;
    for (;;) {
      key_in = getc(stdin);
      if (key_in > 0) break;
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    switch (key_in) {
      case '1':
        TestPlatform();
        break;
      case '2':
        TestRadioCw(TARGET_SX1261);
        break;
      case '3':
        TestRadioTx(TARGET_SX1261);
        break;
      case '4':
        TestRadioRx(TARGET_SX1261);
        break;
      case '5':
        TestRadioCw(TARGET_SX1280);
        break;
      case '6':
        TestRadioTx(TARGET_SX1280);
        break;
      case '7':
        TestRadioRx(TARGET_SX1280);
        break;
      case '8':
        TestLoRaComponent();
        break;
      default:
        if (key_in >= 0x20) {
          PrintLine("ERROR. Unknow selection %c (0x%02X)", key_in, key_in);
        }
        break;
    }
  }
}