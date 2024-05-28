//==========================================================================
//==========================================================================
#ifndef INC_BUTTON_H
#define INC_BUTTON_H

//==========================================================================
//==========================================================================
#include <stdint.h>
#include <stdbool.h>

//==========================================================================
//==========================================================================
int8_t ButtonInit(void);
bool ButtonIsPressedOnce (void);
void ButtonClear (void);
bool ButtonIsHeld (uint32_t aTime);
bool ButtonIsPressed (void);

//==========================================================================
//==========================================================================
#endif // INC_BUTTON_H
