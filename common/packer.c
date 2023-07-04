//==================================================================================================
// Print
//  Author: Ian Lee
//  History:
//    2019-DEC-8  Created.
//==================================================================================================
#include "packer.h"

//==========================================================================
// Buffer packing utils
//==========================================================================
uint16_t PackFloat(uint8_t *aDest, float32_t aValue) {
  const uint8_t *ptr = (const uint8_t *)(&aValue);

  aDest[3] = ptr[0];
  aDest[2] = ptr[1]; /*lint !e2662 Suppress warning */
  aDest[1] = ptr[2]; /*lint !e2662 Suppress warning */
  aDest[0] = ptr[3]; /*lint !e2662 Suppress warning */

  return 4;
}

uint16_t PackU32(uint8_t *aDest, uint32_t aValue) {
  aDest[0] = (uint8_t)(aValue >> 24);
  aDest[1] = (uint8_t)(aValue >> 16);
  aDest[2] = (uint8_t)(aValue >> 8);
  aDest[3] = (uint8_t)(aValue);

  return 4;
}

uint16_t PackU16(uint8_t *aDest, uint16_t aValue) {
  aDest[0] = (uint8_t)(aValue >> 8);
  aDest[1] = (uint8_t)(aValue);

  return 2;
}

uint16_t PackU8(uint8_t *aDest, uint8_t aValue) {
  *aDest = aValue;

  return 1;
}

uint16_t PackString(uint8_t *aDest, const char *aSrc, uint16_t aLen) {
  uint16_t idx = 0;
  const char *ptr = aSrc;

  while (idx < aLen) {
    if (*ptr != (char)0) {
      char ch = *ptr;
      aDest[idx] = ch; /*lint !e732 !e9034 MISRA 2012 Rule 10.3 */
      ptr++;
    } else {
      aDest[idx] = 0;
    }
    idx++;
  }

  return idx;
}

//==========================================================================
// Buffer Unpack utils
//==========================================================================
float32_t UnpackFloat(const uint8_t *aSrc) {
  float32_t f;
  uint8_t *ptr = (uint8_t *)(&f);

  ptr[0] = aSrc[3]; /*lint !e2662 Suppress warning */
  ptr[1] = aSrc[2]; /*lint !e2662 Suppress warning */
  ptr[2] = aSrc[1]; /*lint !e2662 Suppress warning */
  ptr[3] = aSrc[0]; /*lint !e2662 Suppress warning */

  return f;
}

uint32_t UnpackU32(const uint8_t *aSrc) {
  uint32_t ret;
  const uint8_t *ptr = aSrc;

  ret = *ptr;
  ret <<= 8;
  ptr++;
  ret |= *ptr;
  ret <<= 8;
  ptr++;
  ret |= *ptr;
  ret <<= 8;
  ptr++;
  ret |= *ptr;

  return ret;
}

uint16_t UnpackU16(const uint8_t *aSrc) {
  uint16_t ret;
  const uint8_t *ptr = aSrc;

  ret = *ptr;
  ret <<= 8;
  ptr++;
  ret |= *ptr;

  return ret;
}

uint8_t UnpackU8(const uint8_t *aSrc) { return *aSrc; }

void UnpackString(char *aDest, const uint8_t *aSrc, uint16_t aLen) {
  uint16_t idx = 0;
  const char *ptr = (const char *)(aSrc);

  while (idx < aLen) {
    if (*ptr != (char)0) {
      aDest[idx] = *ptr;
      ptr++;
    } else {
      aDest[idx] = (char)0;
    }
    idx++;
  }
}

//==========================================================================
// Little Endian pack/unpack
//==========================================================================
uint16_t PackU32Le(uint8_t *aDest, uint32_t aValue) {
  aDest[3] = (uint8_t)(aValue >> 24);
  aDest[2] = (uint8_t)(aValue >> 16);
  aDest[1] = (uint8_t)(aValue >> 8);
  aDest[0] = (uint8_t)(aValue);

  return 4;
}

uint16_t PackU16Le(uint8_t *aDest, uint16_t aValue) {
  aDest[1] = (uint8_t)(aValue >> 8);
  aDest[0] = (uint8_t)(aValue);

  return 2;
}

uint32_t UnpackU32Le(const uint8_t *aSrc) {
  uint32_t ret;
  const uint8_t *ptr = aSrc;

  ret = (uint32_t)ptr[3];
  ret <<= 8;
  ret |= (uint32_t)ptr[2];
  ret <<= 8;
  ret |= (uint32_t)ptr[1];
  ret <<= 8;
  ret |= (uint32_t)ptr[0];

  return ret;
}

uint16_t UnpackU16Le(const uint8_t *aSrc) {
  uint16_t ret;
  const uint8_t *ptr = aSrc;

  ret = (uint16_t)ptr[1];
  ret <<= 8;
  ret |= (uint16_t)ptr[0];

  return ret;
}
