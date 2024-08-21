//==========================================================================
//==========================================================================
#ifndef INC_NFCCOMPON_DEBUG_H
#define INC_NFCCOMPON_DEBUG_H

//==========================================================================
//==========================================================================
#if defined(CONFIG_NFCCOMPON_DEBUG)
#define NFCCOMPON_DEBUG 1
#else
#define NFCCOMPON_DEBUG 0
#endif

#if NFCCOMPON_DEBUG
#include <stdint.h>
#include <stdio.h>

void NfcComponDebugPrintLine(const char *szFormat, ...);
void NfcComponDebugHex2String(const char *aPrefix, const uint8_t *aSrc, int aLen);

#define NFCCOMPON_PRINTLINE(x...) NfcComponDebugPrintLine(x)
#define NFCCOMPON_HEX2STRING(x, y, z) NfcComponDebugHex2String(x, y, z)

#else  // NFCCOMPON_DEBUG

#define NFCCOMPON_PRINTLINE(...) \
  {}
#define NFCCOMPON_HEX2STRING(...) \
  {}

#endif  // NFCCOMPON_DEBUG

//==========================================================================
//==========================================================================

//==========================================================================
//==========================================================================
#endif // INC_NFCCOMPON_DEBUG_H
