//==========================================================================
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
#ifndef INC_APP_UTILS_H
#define INC_APP_UTILS_H

//==========================================================================
//==========================================================================
#include <stdint.h>

//==========================================================================
//==========================================================================
void PrintLine(char *szFormat, ...);
void Hex2String(const char *aPrefix, const unsigned char *aSrc, int aLen);

//==========================================================================
//==========================================================================
uint32_t GetTick(void);
uint32_t TickElapsed(uint32_t aTick);
void TrimString(char *aStr);

//==========================================================================
//==========================================================================
#endif