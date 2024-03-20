//==========================================================================
// Utilities
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
#include "app_utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
// Print functions
//==========================================================================
void PrintLine(char *szFormat, ...) {
  va_list lpStart;
  va_start(lpStart, szFormat);
  vprintf(szFormat, lpStart);
  printf("\n");
  va_end(lpStart);
}

void Hex2String(const char *aPrefix, const unsigned char *aSrc, int aLen) {
  printf("%s", aPrefix);
  for (int i = 0; i < aLen; i++) {
    printf("%02X ", *aSrc);
    aSrc++;
  }
  printf("\n");
}

//==========================================================================
// Get 1ms tick counter
//==========================================================================
uint32_t GetTick(void) {
  uint32_t ret;
  struct timeval curr_time;

  gettimeofday(&curr_time, NULL);

  ret = curr_time.tv_usec / 1000;
  ret += (curr_time.tv_sec * 1000);

  return ret;
}

//==========================================================================
// Calculate the number of tick elapsed
//==========================================================================
uint32_t TickElapsed(uint32_t aTick) {
  uint32_t curr_tick;

  curr_tick = GetTick();
  if (curr_tick >= aTick)
    return (curr_tick - aTick);
  else
    return (0xffffffff - aTick + curr_tick);
}

//==========================================================================
// Trim out leading and tailing space
//==========================================================================
void TrimString(char *aStr) {
  char *ptr;

  // Remove leading space
  ptr = aStr;
  while (*ptr) {
    if (*ptr != ' ') break;
    ptr++;
  }
  if ((*ptr) && (ptr != aStr)) {
    memmove(aStr, ptr, strlen(ptr) + 1);
  }

  // Remove tailing space
  ptr = aStr;
  ptr += (strlen(aStr) + 1);
  while (ptr != aStr) {
    ptr--;
    if (*ptr != ' ') {
      ptr[1] = 0;
      break;
    }
  }
}