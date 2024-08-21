//==========================================================================
// Font 6x8
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
#include <stdio.h>

#include "display.h"
#include "display_hal.h"
#include "LCD-fonts/6x8_vertikal_LSB_2.h"

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Draw a char
//==========================================================================
void Font6x8DrawChar(uint32_t aX, uint32_t aY, uint32_t aFgColor, uint32_t aBgColor, uint8_t aCh) {
  for (uint8_t i = 0; i < 6; i ++) {
    uint8_t pixels = font[aCh][i];
    for (uint8_t j = 0; j < 8; j ++) {
      if (pixels & 0x01) {
        DisplayHalWrPixel(aX + i, aY + j, aFgColor);
      }
      else {
        DisplayHalWrPixel(aX + i, aY + j, aBgColor);
      }
      pixels >>= 1;
    }
  }
}

