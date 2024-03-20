//==========================================================================
//==========================================================================
#ifndef INC_APP_CRC32_H
#define INC_APP_CRC32_H
//==========================================================================
//==========================================================================
#include <stdint.h>

//==========================================================================
//==========================================================================
uint32_t Crc32(uint32_t aCrc, uint8_t aData);
uint32_t Crc32Cal(uint32_t aCrc, uint8_t *aData, uint32_t aLen);
uint32_t Crc32Finalize(uint32_t aCrc);

//==========================================================================
//==========================================================================
#endif