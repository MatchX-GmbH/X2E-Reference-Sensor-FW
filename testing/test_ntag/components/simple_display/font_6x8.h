//==========================================================================
//==========================================================================
#ifndef INC_FONT_6x8_H
#define INC_FONT_6x8_H

//==========================================================================
//==========================================================================
#include <stdint.h>

//==========================================================================
//==========================================================================
#define Font6x8Width() (6)
#define Font6x8Height() (8)

//==========================================================================
//==========================================================================
void Font6x8DrawChar(uint32_t aX, uint32_t aY, uint32_t aFgColor, uint32_t aBgColor, uint8_t aCh);

//==========================================================================
//==========================================================================
#endif  // INC_FONT_6x8_H