//==========================================================================
//==========================================================================
#ifndef INC_BOARD_H
#define INC_BOARD_H

//==========================================================================
//==========================================================================
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sdkconfig.h"

//==========================================================================
//==========================================================================
// SPI
#define SPIHOST SPI3_HOST

// I2C
#define I2CNUM I2C_NUM_0

//==========================================================================
//==========================================================================
void MxTargetInit(void);
const char *MxTargetName(void);
void MxTargetPrepareForSleep(void);
void MxTargetResumeFromSleep(void);

//==========================================================================
//==========================================================================
#endif  // INC_BOARD_H