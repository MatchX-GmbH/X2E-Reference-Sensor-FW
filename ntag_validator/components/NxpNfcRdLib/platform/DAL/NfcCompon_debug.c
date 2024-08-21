
//==========================================================================
//==========================================================================
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include "NfcCompon_debug.h"

//==========================================================================
//==========================================================================
#if NFCCOMPON_DEBUG

static const char *kDebugPrefix = "[NFC]";

//==========================================================================
//==========================================================================
void NfcComponDebugPrintLine(const char *szFormat, ...) {
  va_list lpStart;
  va_start(lpStart, szFormat);
  printf(kDebugPrefix);
  vprintf(szFormat, lpStart);
  printf("\n");
  va_end(lpStart);
}

void NfcComponDebugHex2String(const char *aPrefix, const uint8_t *aSrc, int aLen) {
  printf(kDebugPrefix);
  printf("%s", aPrefix);
  while (aLen) {
    printf(" %02X", *aSrc);
    aLen--;
    aSrc++;
  }
  printf("\n");
}

//==========================================================================
//==========================================================================
#endif  // LORA_COMPON_DEBUG
