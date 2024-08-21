//==========================================================================
//==========================================================================
#ifndef INC_DISPLAY_H
#define INC_DISPLAY_H

//==========================================================================
//==========================================================================
#include <stdint.h>

//==========================================================================
//==========================================================================
typedef enum {
  DISPLAY_FONT_6x8 = 0,
  DISPLAY_FONT_8x12,
  DISPLAY_FONT_12x16,
} DisplayFont_t;

//==========================================================================
//==========================================================================
int8_t DisplayInit(void);

void DisplaySetFont(DisplayFont_t aFont);
void DisplaySetColor(uint32_t aForeground, uint32_t aBackground);

void DisplayClearScreen(void);
void DisplayFillScreen(uint32_t aColor);
void DisplayDrawGrayArray(uint32_t aX, uint32_t aY, uint32_t aWitdh, uint32_t aHeight, const uint8_t *aData);
void DisplayDrawString(uint32_t aX, uint32_t aY, const char *aStr);
void DisplayDrawLine(uint32_t aX0, uint32_t aY0, uint32_t aX1, uint32_t aY1);
void DisplayDrawRect(uint32_t aX0, uint32_t aY0, uint32_t aX1, uint32_t aY1);

//==========================================================================
//==========================================================================
#endif // INC_DISPLAY_H