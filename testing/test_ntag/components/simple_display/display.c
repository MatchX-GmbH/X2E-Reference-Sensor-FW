//==========================================================================
// A simple Display
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
#include "display.h"

#include <stdio.h>

#include "display_hal.h"
#include "font_12x16.h"
#include "font_6x8.h"
#include "font_8x12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mutex_helper.h"

//==========================================================================
// Defines
//==========================================================================
#define TASK_SLEEP_TIME 200  // about 5 frame/sec
#define TASK_STACK_SIZE 2048
#define TASK_PRIO 8

//==========================================================================
// Variables
//==========================================================================
static uint32_t gForegroundColor;
static uint32_t gBackgroundColor;
static DisplayFont_t gCurrentFont;

// Task info
static TaskHandle_t gMyHangle;

//==========================================================================
// Display Task
//==========================================================================
static void DisplayTask(void *pvParameters) {
  //
  while (1) {
    vTaskDelay(TASK_SLEEP_TIME / portTICK_PERIOD_MS);
    if (TakeMutex()) {
      DisplayHalUpdate();
      FreeMutex();
    }
  }
}

//==========================================================================
// Init
//==========================================================================
int8_t DisplayInit(void) {
  if (DisplayHalInit() < 0) {
    printf("ERROR. DisplayInit failed. HAL init error.\n");
    return -1;
  }
  // Init variables
  InitMutex();
  gMyHangle = NULL;

  gForegroundColor = 0xffffff;
  gBackgroundColor = 0x000000;
  gCurrentFont = DISPLAY_FONT_6x8;

  //
  xTaskCreate(DisplayTask, "DisplayTask", TASK_STACK_SIZE, NULL, TASK_PRIO, &gMyHangle);

  if (gMyHangle == NULL) {
    printf("ERROR. DisplayInit xTaskCreate() failed.\n");
    return -1;
  }

  return 0;
}

//==========================================================================
// Set display parameters
//==========================================================================
void DisplaySetFont(DisplayFont_t aFont) { gCurrentFont = aFont; }
void DisplaySetColor(uint32_t aForeground, uint32_t aBackground) {
  gForegroundColor = aForeground;
  gBackgroundColor = aBackground;
}

//==========================================================================
// Clear whole screen
//==========================================================================
void DisplayClearScreen(void) {
  if (TakeMutex()) {
    DisplayHalFillAll(0);
    FreeMutex();
  }
}

//==========================================================================
// Fill whole screen
//==========================================================================
void DisplayFillScreen(uint32_t aColor) {
  if (TakeMutex()) {
    DisplayHalFillAll(aColor);
    FreeMutex();
  }
}

//==========================================================================
// Draw a array of mono data (uint8_t 0=Background, 1=Foreground )
//==========================================================================
void DisplayDrawGrayArray(uint32_t aX, uint32_t aY, uint32_t aWitdh, uint32_t aHeight, const uint8_t *aData) {
  if (TakeMutex()) {
    for (uint32_t y = 0; y < aHeight; y++) {
      for (uint32_t x = 0; x < aWitdh; x++) {
        if (*aData < 128) {
          DisplayHalWrPixel(aX + x, aY + y, gBackgroundColor);
        } else {
          DisplayHalWrPixel(aX + x, aY + y, gForegroundColor);
        }
        aData++;
      }
    }
    FreeMutex();
  }
}

//==========================================================================
// Draw a string
//==========================================================================
void DisplayDrawString(uint32_t aX, uint32_t aY, const char *aStr) {
  if (TakeMutex()) {
    while (*aStr) {
      uint8_t font_width;
      uint8_t font_height;

      if (gCurrentFont == DISPLAY_FONT_8x12) {
        font_width = Font8x12Width();
        font_height = Font8x12Height();
      } else if (gCurrentFont == DISPLAY_FONT_12x16) {
        font_width = Font12x16Width();
        font_height = Font12x16Height();
      } else {
        font_width = Font6x8Width();
        font_height = Font6x8Height();
      }

      if ((aX + font_width) >= DisplayhalWidth()) {
        break;
      }
      if ((aY + font_height) >= DisplayhalHeight()) {
        break;
      }

      if (gCurrentFont == DISPLAY_FONT_8x12) {
        Font8x12DrawChar(aX, aY, gForegroundColor, gBackgroundColor, *aStr);
      } else if (gCurrentFont == DISPLAY_FONT_12x16) {
        Font12x16DrawChar(aX, aY, gForegroundColor, gBackgroundColor, *aStr);
      } else {
        Font6x8DrawChar(aX, aY, gForegroundColor, gBackgroundColor, *aStr);
      }

      aX += font_width;
      aStr++;
    }
    FreeMutex();
  }
}

//==========================================================================
// Draw a line
//==========================================================================
void DisplayDrawLine(uint32_t aX0, uint32_t aY0, uint32_t aX1, uint32_t aY1) {
  int32_t dy, dx, inc_e, inc_ne, d;
  uint32_t x, y;
  bool inverted_y = false;
  bool swap_xy = false;
  ;

  dx = aX0 - aX1;
  dy = aY0 - aY1;
  if (dx < 0) dx = dx * -1;
  if (dy < 0) dy = dy * -1;
  if (dy > dx) {
    swap_xy = true;
    x = aX0;
    aX0 = aY0;
    aY0 = x;
    x = aX1;
    aX1 = aY1;
    aY1 = x;
  }

  if (aX0 > aX1) {
    x = aX0;
    y = aY0;
    aX0 = aX1;
    aY0 = aY1;
    aX1 = x;
    aY1 = y;
  }
  dx = aX1 - aX0;

  if (aY1 >= aY0) {
    dy = aY1 - aY0;
  } else {
    dy = aY0 - aY1;
    inverted_y = true;
  }

  d = 2 * dy - dx;
  inc_e = 2 * dy;
  inc_ne = 2 * (dy - dx);
  x = aX0;
  y = aY0;

  if (TakeMutex()) {
    if (swap_xy) {
      DisplayHalWrPixel(y, x, gForegroundColor);
    } else {
      DisplayHalWrPixel(x, y, gForegroundColor);
    }

    while (x < aX1) {
      if (d <= 0) {
        d += inc_e;
        x++;
      } else {
        d += inc_ne;
        x++;
        if (inverted_y) {
          y--;
        } else {
          y++;
        }
      }
      if (swap_xy) {
        DisplayHalWrPixel(y, x, gForegroundColor);
      } else {
        DisplayHalWrPixel(x, y, gForegroundColor);
      }
    }
    FreeMutex();
  }
}

//==========================================================================
// Draw a rectangle
//==========================================================================
void DisplayDrawRect(uint32_t aX0, uint32_t aY0, uint32_t aX1, uint32_t aY1) {
  DisplayDrawLine(aX0, aY0, aX1, aY0);
  DisplayDrawLine(aX1, aY0, aX1, aY1);
  DisplayDrawLine(aX0, aY0, aX0, aY1);
  DisplayDrawLine(aX0, aY1, aX1, aY1);
}
