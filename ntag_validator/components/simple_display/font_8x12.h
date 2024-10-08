//==========================================================================
//==========================================================================
#ifndef INC_FONT_8x12_H
#define INC_FONT_8x12_H

//==========================================================================
//==========================================================================
#include <stdint.h>

//==========================================================================
//==========================================================================
#define Font8x12Width() (8)
#define Font8x12Height() (12)

//==========================================================================
//==========================================================================
void Font8x12DrawChar(uint32_t aX, uint32_t aY, uint32_t aFgColor, uint32_t aBgColor, uint8_t aCh);

//==========================================================================
//==========================================================================
#endif  // INC_FONT_8x12_H