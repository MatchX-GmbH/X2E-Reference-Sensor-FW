//==========================================================================
// Application Data
//==========================================================================
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
// Naming conventions
// ~~~~~~~~~~~~~~~~~~
//                Class : Leading C
//               Struct : Leading T
//       typedef Struct : tailing _t
//             Constant : Leading k
//      Global Variable : Leading g
//    Function argument : Leading a
//       Local Variable : All lower case
//==========================================================================
#include "app_data.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "mutex_helper.h"
#include "mx_target.h"
#include "nvs_flash.h"

//==========================================================================
// Defines
//==========================================================================
// Key list, max 15 character
#define KEY_APP_DATA "app_data"
#define KEY_APP_SETTINGS "app_settings"

// Magic code
#define MAGIC_APP_DATA 0x159d265b
#define MAGIC_APP_SETTINGS 0x847b3ef9

//==========================================================================
// Variables
//==========================================================================
static bool gNvsReady;
static AppData_t gAppData;
static AppSettings_t gAppSettings;

//==========================================================================
// Constants
//==========================================================================
static const char *kStorageNamespace = "MatchX";

//==========================================================================
// Reset to default
//==========================================================================
static void SetDefaultAppData(void) {
  DEBUG_PRINTLINE("Set app data to default.");
  memset(&gAppData, 0, sizeof(AppData_t));
  gAppData.magicCode = MAGIC_APP_DATA;
}

static void SetDefaultAppSettings(void) {
  DEBUG_PRINTLINE("Set app settings to default.");
  memset(&gAppSettings, 0, sizeof(AppSettings_t));
  gAppSettings.magicCode = MAGIC_APP_SETTINGS;
  strncpy(gAppSettings.qrCode, "{\"PID\":\"X\"}", sizeof(gAppSettings.qrCode));
}

//==========================================================================
// Calculate XOR value
//==========================================================================
static uint8_t CalXorValue(const void *aData, uint32_t aLen) {
  const uint8_t *ptr = (const uint8_t *)aData;
  uint8_t cal_xor = 0xAA;  // Initial value
  while (aLen > 0) {
    cal_xor ^= *ptr;
    ptr++;
    aLen--;
  }
  return cal_xor;
}

//==========================================================================
// Init
//   nvs_flash_init() must called before this.
//==========================================================================
int8_t AppDataInit(void) {
  esp_err_t esp_ret;

  //
  gNvsReady = false;

  // Init variables
  InitMutex();

  //
  nvs_handle h_matchx;

  esp_ret = nvs_open(kStorageNamespace, NVS_READONLY, &h_matchx);
  if (esp_ret == ESP_ERR_NVS_NOT_FOUND) {
    PrintLine("WARNING. AppDataInit namespace not found. Create.");
    // No namespace, create one
    SetDefaultAppData();
    SetDefaultAppSettings();
    gNvsReady = true;
    if ((AppDataSaveToStorage() < 0) || (AppSettingsSaveToStorage() < 0)) {
      gNvsReady = false;
      return -1;
    } else {
      return 0;
    }
  } else if (esp_ret != ESP_OK) {
    PrintLine("ERROR. AppDataInit nvs_open() failed. %s.", esp_err_to_name(esp_ret));
    return -1;
  } else {
    esp_err_t esp_ret;
    size_t len;

    // gAppData
    len = sizeof(gAppData);
    esp_ret = nvs_get_blob(h_matchx, KEY_APP_DATA, &gAppData, &len);
    if (esp_ret != ESP_OK) {
      PrintLine("ERROR. AppDataInit get %s failed. %s.", KEY_APP_DATA, esp_err_to_name(esp_ret));
      SetDefaultAppData();
      AppDataSaveToStorage();
    } else {
      PrintLine("INFO. Read AppData success.");
      gNvsReady = true;

      // Check XOR
      uint8_t cal_xor = CalXorValue(&gAppData, sizeof(gAppData));
      if (cal_xor != 0) {
        PrintLine("WARNING. Invalid AppData, set to default.");
        SetDefaultAppData();
        AppDataSaveToStorage();
      }
    }

    // gAppSettings
    len = sizeof(gAppSettings);
    esp_ret = nvs_get_blob(h_matchx, KEY_APP_SETTINGS, &gAppSettings, &len);
    if (esp_ret != ESP_OK) {
      PrintLine("ERROR. AppDataInit get %s failed. %s.", KEY_APP_SETTINGS, esp_err_to_name(esp_ret));
      SetDefaultAppSettings();
      AppSettingsSaveToStorage();
    } else {
      PrintLine("INFO. Read AppSettings success.");
      gNvsReady = true;

      // Check XOR
      uint8_t cal_xor = CalXorValue(&gAppSettings, sizeof(gAppSettings));
      if (cal_xor != 0) {
        PrintLine("WARNING. Invalid AppSettings, set to default.");
        SetDefaultAppSettings();
        AppSettingsSaveToStorage();
      }
    }

    //
    nvs_close(h_matchx);
    return 0;
  }
}

//==========================================================================
//==========================================================================
AppData_t *AppDataGet(void) { return &gAppData; }

//==========================================================================
// Save AppData to storage
//==========================================================================
int8_t AppDataSaveToStorage(void) {
  esp_err_t esp_ret;
  nvs_handle h_matchx;

  // Check NVS is ready
  if (!gNvsReady) {
    PrintLine("ERROR. AppDataSaveToStorage failed. NVS not ready.");
    return -1;
  }

  // Open storage
  esp_ret = nvs_open(kStorageNamespace, NVS_READWRITE, &h_matchx);
  if (esp_ret != ESP_OK) {
    PrintLine("ERROR. AppDataSaveToStorage nvs_open(), %s.", esp_err_to_name(esp_ret));
    return -1;
  } else {
    int ret = -1;
    if (TakeMutex()) {
      // Update XOR
      gAppData.xorValue = 0;
      uint8_t cal_xor = CalXorValue(&gAppData, sizeof(gAppData));
      gAppData.xorValue = cal_xor;

      //
      esp_err_t esp_ret = nvs_set_blob(h_matchx, KEY_APP_DATA, &gAppData, sizeof(gAppData));
      if (esp_ret != ESP_OK) {
        PrintLine("ERROR. AppDataSaveToStorage set %s failed. %s.", KEY_APP_DATA, esp_err_to_name(esp_ret));
      } else {
        // All done
        ret = 0;
      }
      FreeMutex();
    }

    // Commit and close
    if (nvs_commit(h_matchx) != ESP_OK) {
      PrintLine("ERROR. AppDataSaveToStorage nvs commit failed. %s.", esp_err_to_name(ret));
    }
    nvs_close(h_matchx);
    return ret;
  }
}

//==========================================================================
//==========================================================================
void AppDataResetToDefault(void) {
  if (TakeMutex()) {
    SetDefaultAppData();
    FreeMutex();

    AppDataSaveToStorage();
  }
}

//==========================================================================
//==========================================================================
AppSettings_t *AppSettingsGet(void) { return &gAppSettings; }

//==========================================================================
// Save AppSettings to storage
//==========================================================================
int8_t AppSettingsSaveToStorage(void) {
  esp_err_t esp_ret;
  nvs_handle h_matchx;

  // Check NVS is ready
  if (!gNvsReady) {
    PrintLine("ERROR. AppSettingsSaveToStorage failed. NVS not ready.");
    return -1;
  }

  // Open storage
  esp_ret = nvs_open(kStorageNamespace, NVS_READWRITE, &h_matchx);
  if (esp_ret != ESP_OK) {
    PrintLine("ERROR. AppSettingsSaveToStorage nvs_open(), %s.", esp_err_to_name(esp_ret));
    return -1;
  } else {
    int ret = -1;
    if (TakeMutex()) {
      // Update XOR
      gAppSettings.xorValue = 0;
      uint8_t cal_xor = CalXorValue(&gAppSettings, sizeof(gAppSettings));
      gAppSettings.xorValue = cal_xor;

      //
      esp_err_t esp_ret = nvs_set_blob(h_matchx, KEY_APP_SETTINGS, &gAppSettings, sizeof(gAppSettings));
      if (esp_ret != ESP_OK) {
        PrintLine("ERROR. AppSettingsSaveToStorage set %s failed. %s.", KEY_APP_SETTINGS, esp_err_to_name(esp_ret));
      } else {
        // All done
        ret = 0;
      }
      FreeMutex();
    }

    // Commit and close
    if (nvs_commit(h_matchx) != ESP_OK) {
      PrintLine("ERROR. AppSettingsSaveToStorage nvs commit failed. %s.", esp_err_to_name(ret));
    }
    nvs_close(h_matchx);
    return ret;
  }
}

//==========================================================================
//==========================================================================
void AppSettingsResetToDefault(void) {
  if (TakeMutex()) {
    SetDefaultAppSettings();
    FreeMutex();

    AppSettingsSaveToStorage();
  }
}

//==========================================================================
// Mutex for safe access of AppData and AppSettings
//==========================================================================
bool AppDataTakeMutex(void) { return TakeMutex(); }

void AppDataFreeMutex(void) { FreeMutex(); }
