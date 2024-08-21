//==========================================================================
//==========================================================================
#ifndef INC_DISPLAY_HAL_H
#define INC_DISPLAY_HAL_H

//==========================================================================
//==========================================================================
#include <stdint.h>
#include <stdbool.h>

//==========================================================================
// Color format 24bits  0x00RRGGBB
//==========================================================================
int8_t DisplayHalInit(bool aClearScreen);
uint16_t DisplayhalWidth(void);
uint16_t DisplayhalHeight(void);
void DisplayHalFillAll(uint32_t aColor);
void DisplayHalWrPixel(uint16_t aX, uint16_t aY, uint32_t aColor);
uint32_t DisplayHalRdPixel(uint16_t aX, uint16_t aY);
void DisplayHalUpdate(void);

void DisplayHalDeInit(void);

//==========================================================================
//==========================================================================
#endif // INC_DISPLAY_HAL_H