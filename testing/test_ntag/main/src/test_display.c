//==========================================================================
// Test Display
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
#include "test_display.h"

#include <stdio.h>

#include "app_utils.h"
#include "debug.h"
#include "display.h"
#include "logo_matchx.h"
#include "mx_target.h"

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
// Test Display
//==========================================================================
void TestDisplay(void) {
  //
  PrintLine("TestDisplay");
  DisplayInit();

  // Draw logo and text
  DisplayClearScreen();
  DisplayDrawGrayArray(0, 8, LOGO_WIDTH, LOGO_HEIGHT, kLogoData);
  DisplaySetFont(DISPLAY_FONT_12x16);
  DisplayDrawString(49, 24, "MatchX");
  DisplaySetFont(DISPLAY_FONT_8x12);
  DisplayDrawString(95, 40, "GmbH");
  DisplayDrawLine(50, 52, 127, 63);
  DisplayDrawLine(50, 63, 127, 52);
  DisplayDrawLine(97, 52, 80, 63);
  DisplayDrawLine(97, 63, 80, 52);
  DisplayDrawLine(87, 63, 90, 52);
  DisplayDrawLine(90, 63, 87, 52);

  PrintLine("Press [ENTER] to exit.");
  FlushInput();
  for (;;) {
    // Draw Tick count
    char buf[12];
    uint32_t tick = GetTick();

    snprintf(buf, sizeof(buf), "%10d", GetTick());
    DisplaySetFont(DISPLAY_FONT_6x8);
    if (((tick / 1000) & 0x01) != 0) {
      DisplaySetColor(0x000000, 0xffffff);
      DisplayDrawRect(64, 1, 125, 10);
      DisplaySetColor(0xffffff, 0x000000);
      DisplayDrawString(65, 2, buf);
    }
    else {
      DisplaySetColor(0xffffff, 0x000000);
      DisplayDrawRect(64, 1, 125, 10);
      DisplaySetColor(0x000000, 0xffffff);
      DisplayDrawString(65, 2, buf);
    }
    DisplaySetColor(0xffffff, 0x000000);
    DisplayDrawRect(63, 0, 126, 11);

    // Exit if ENTER pressed
    int key_in = getc(stdin);
    if (key_in >= 0) {
      DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
    }
    if ((key_in == 0x0a) || (key_in == 0x0d)) {
      break;
    }

    //
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }

  //
  DisplaySetColor(0xffffff, 0x000000);
  DisplayClearScreen();
  DisplaySetFont(DISPLAY_FONT_6x8);
  DisplayDrawString(0,0,"Display test done.");

}