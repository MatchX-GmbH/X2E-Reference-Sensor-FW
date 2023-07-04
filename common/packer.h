//==========================================================================
// Buffer packing utils
//==========================================================================
#ifndef INC_PACKER_H
#define INC_PACKER_H
//==========================================================================
//==========================================================================
#include "define_type.h"

//==========================================================================
//==========================================================================
uint16_t PackFloat (uint8_t *aDest, float32_t aValue);
uint16_t PackU32 (uint8_t *aDest, uint32_t aValue);
uint16_t PackU16 (uint8_t *aDest, uint16_t aValue);
uint16_t PackU8 (uint8_t *aDest, uint8_t aValue);
uint16_t PackString (uint8_t *aDest, const char *aSrc, uint16_t aLen);

//==========================================================================
// Buffer Unpack utils
//==========================================================================
float32_t UnpackFloat (const uint8_t *aSrc);
uint32_t UnpackU32 (const uint8_t *aSrc);
uint16_t UnpackU16 (const uint8_t *aSrc);
uint8_t UnpackU8 (const uint8_t *aSrc);
void UnpackString (char *aDest, const uint8_t *aSrc, uint16_t aLen);

//==========================================================================
// Little-endian
//==========================================================================
uint16_t PackU32Le (uint8_t *aDest, uint32_t aValue);
uint16_t PackU16Le (uint8_t *aDest, uint16_t aValue);
uint32_t UnpackU32Le (const uint8_t *aSrc);
uint16_t UnpackU16Le (const uint8_t *aSrc);

//==========================================================================
//==========================================================================
#endif
