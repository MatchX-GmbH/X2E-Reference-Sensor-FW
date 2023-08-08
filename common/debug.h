//==========================================================================
//==========================================================================
#ifndef INC_DEBUG_H
#define INC_DEBUG_H

//==========================================================================
//==========================================================================
#include "sdkconfig.h"

//==========================================================================
//==========================================================================
#if defined(CONFIG_MATCHX_DEBUG)
#define DEBUG 1
#else
#define DEBUG 0
#endif

//==========================================================================
//==========================================================================
#if (DEBUG)

#define DEBUG_INIT()            DebugInit ()
#define DEBUG_EXIT()            DebugExit ()
#define DEBUG_PRINTLINE(x...)   DebugPrintLine (x)
#define DEBUG_HEX2STRING(x,y,z)   DebugHex2String (x,y,z)

#else   // #if (DEBUG)

#define DEBUG_INIT()            {}
#define DEBUG_EXIT()            {}
#define DEBUG_PRINTLINE(x...)      {}
#define DEBUG_HEX2STRING(x,y,z)   {}

#endif   // #if (DEBUG)


//==========================================================================
//==========================================================================
void DebugInit (void);
void DebugExit (void);
void DebugPrintLine (const char *aMsg, ...);
void DebugHex2String (const char *aPrefix, const unsigned char *aSrc, int aLen);

//==========================================================================
//==========================================================================
#endif
