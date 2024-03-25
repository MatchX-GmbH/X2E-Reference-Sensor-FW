//==========================================================================
//==========================================================================
#ifndef INC_APP_DATA_H
#define INC_APP_DATA_H

//==========================================================================
//==========================================================================
#include <stdint.h>
#include <stdbool.h>

//==========================================================================
//==========================================================================
typedef struct {
  uint32_t magicCode;
  uint8_t xorValue;
}AppData_t;

typedef struct {
  uint32_t magicCode;
  char qrCode[128];
  uint8_t xorValue;
}AppSettings_t;

//==========================================================================
//==========================================================================
int8_t AppDataInit(void);

AppData_t *AppDataGet (void);
int8_t AppDataSaveToStorage(void);
void AppDataResetToDefault(void);

AppSettings_t *AppSettingsGet (void);
int8_t AppSettingsSaveToStorage(void);
void AppSettingsResetToDefault(void);

bool AppDataTakeMutex(void);
void AppDataFreeMutex(void);

//==========================================================================
//==========================================================================
#endif  // INC_APP_DATA_H
