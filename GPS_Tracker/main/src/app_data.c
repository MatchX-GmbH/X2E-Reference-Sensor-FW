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

#include "debug.h"
#include "mutex_helper.h"
#include "mx_target.h"
#include "nvs_flash.h"

//==========================================================================
// Defines
//==========================================================================
// Key list, max 15 character
#define KEY_DISTANCE "distance"

#define MAX_STRING_LEN 127

//==========================================================================
// Variables
//==========================================================================
static bool gNvsReady;
static AppData_t gAppData;
static const char *gLastEspError;

//==========================================================================
// Constants
//==========================================================================
static const char *kStorageNamespace = "MatchX";

//==========================================================================
// Reset to default
//==========================================================================
static void ResetToDefaultValue(void) {
  DEBUG_PRINTLINE("Reset default app data.");
  memset(&gAppData, 0, sizeof(AppData_t));
  gAppData.distance = 0;
}

//==========================================================================
// NVS values
//==========================================================================
static int8_t ReadNvsString(nvs_handle aHandle, const char *aKey, char *aValue, int16_t aValueSize) {
  char str_buf[MAX_STRING_LEN + 1];
  size_t len = sizeof(str_buf);
  esp_err_t esp_ret = nvs_get_str(aHandle, aKey, str_buf, &len);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    memset(aValue, 0, aValueSize);
    return -1;
  }
  strncpy(aValue, str_buf, aValueSize - 1);
  aValue[aValueSize - 1] = 0;
  return 0;
}

static int8_t ReadNvsU32(nvs_handle aHandle, const char *aKey, uint32_t *aValue) {
  esp_err_t esp_ret = nvs_get_u32(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    *aValue = 0;
    return -1;
  }
  return 0;
}

static int8_t ReadNvsInt32(nvs_handle aHandle, const char *aKey, int32_t *aValue) {
  esp_err_t esp_ret = nvs_get_i32(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    *aValue = 0;
    return -1;
  }
  return 0;
}

static int8_t ReadNvsU64(nvs_handle aHandle, const char *aKey, uint64_t *aValue) {
  esp_err_t esp_ret = nvs_get_u64(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    *aValue = 0;
    return -1;
  }
  return 0;
}

static int8_t ReadNvsDouble(nvs_handle aHandle, const char *aKey, double *aValue) {
  size_t required_size = sizeof(double);
  esp_err_t esp_ret = nvs_get_blob(aHandle, aKey, aValue, &required_size);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    *aValue = 0;
    return -1;
  }
  return 0;
}

static int8_t WriteNvsString(nvs_handle aHandle, const char *aKey, char *aValue) {
  char str_buf[MAX_STRING_LEN + 1];
  strncpy(str_buf, aValue, MAX_STRING_LEN);
  str_buf[MAX_STRING_LEN] = 0;
  size_t len = sizeof(str_buf);
  if (len > MAX_STRING_LEN)
    len = MAX_STRING_LEN;
  esp_err_t esp_ret = nvs_set_str(aHandle, aKey, str_buf);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }
  return 0;
}

static int8_t WriteNvsU32(nvs_handle aHandle, const char *aKey, uint32_t aValue) {
  esp_err_t esp_ret = nvs_set_u32(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }
  return 0;
}

static int8_t WriteNvsInt32(nvs_handle aHandle, const char *aKey, int32_t aValue) {
  esp_err_t esp_ret = nvs_set_i32(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }
  return 0;
}

static int8_t WriteNvsU64(nvs_handle aHandle, const char *aKey, uint64_t aValue) {
  esp_err_t esp_ret = nvs_set_u64(aHandle, aKey, aValue);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }

  esp_ret = nvs_commit(aHandle);
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }

  return 0;
}

static int8_t WriteNvsDouble(nvs_handle aHandle, const char *aKey, double aValue) {

  esp_err_t esp_ret = nvs_set_blob(aHandle, aKey, &aValue, sizeof(double));
  if (esp_ret != ESP_OK) {
    gLastEspError = esp_err_to_name(esp_ret);
    return -1;
  }
  return 0;
}

//==========================================================================
// Init
//==========================================================================
int8_t AppDataInit(void) {
  esp_err_t esp_ret;

  //
  gNvsReady = false;
  gLastEspError = "";

  // Init storage
  esp_ret = nvs_flash_init();
  if ((esp_ret == ESP_ERR_NVS_NO_FREE_PAGES) || (esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    printf("WARNING. NVS Full or version changed. Erase.\n");
    esp_ret = nvs_flash_erase();
    if (esp_ret != ESP_OK) {
      printf("ERROR. Erase NVS failed. %s\n", esp_err_to_name(esp_ret));
      return -1;
    }
    esp_ret = nvs_flash_init();
  }
  if (esp_ret != ESP_OK) {
    printf("ERROR. Init NVS failed. %s\n", esp_err_to_name(esp_ret));
    return -1;
  }

  // Init variables
  InitMutex();

  //
  nvs_handle h_matchx;

  esp_ret = nvs_open(kStorageNamespace, NVS_READONLY, &h_matchx);
  if (esp_ret == ESP_ERR_NVS_NOT_FOUND) {
    printf("WARNING. AppDataInit namespace not found. Create.\n");
    // No namespace, create one
    ResetToDefaultValue();
    gNvsReady = true;
    if (AppDataSaveToStorage() < 0) {
      gNvsReady = false;
      return -1;
    } else {
      return 0;
    }
  } else if (esp_ret != ESP_OK) {
    printf("ERROR. AppDataInit nvs_open() failed. %s.\n", esp_err_to_name(esp_ret));
  } else {
    if (ReadNvsDouble(h_matchx, KEY_DISTANCE, &gAppData.distance) < 0) {
      printf("ERROR. AppDataInit read %s failed. %s.\n", KEY_DISTANCE, gLastEspError);
    } else {
      // All done
      gNvsReady = true;
      printf("Read AppData success.\n");
    }
    nvs_close(h_matchx);
  }
  return 0;
}

//==========================================================================
//==========================================================================
AppData_t* AppDataGet(void) {
  return &gAppData;
}

//==========================================================================
// Save config to storage
//==========================================================================
int8_t AppDataSaveToStorage(void) {
  esp_err_t esp_ret;
  nvs_handle h_matchx;

  // Check NVS is ready
  if (!gNvsReady) {
    printf("ERROR. AppDataSaveToStorage failed. NVS not ready.\n");
    return -1;
  }

  // Open storage
  esp_ret = nvs_open(kStorageNamespace, NVS_READWRITE, &h_matchx);
  if (esp_ret != ESP_OK) {
    printf("ERROR. AppDataSaveToStorage nvs_open(), %s.\n", esp_err_to_name(esp_ret));
    return -1;
  } else {
    int ret = -1;
    if (TakeMutex()) {
      if (WriteNvsDouble(h_matchx, KEY_DISTANCE, gAppData.distance) < 0) {
        printf("ERROR. AppDataSaveToStorage set %s failed. %s.\n", KEY_DISTANCE, gLastEspError);
      } else {
        // All done
        ret = 0;
      }
      FreeMutex();
    }

    //
    nvs_close(h_matchx);
    return ret;
  }
}

//==========================================================================
//==========================================================================
void AppDataResetToDefault(void) {
  if (TakeMutex()) {
    ResetToDefaultValue();
    FreeMutex();

    AppDataSaveToStorage();
  }
}

//==========================================================================
// Mutex
//==========================================================================
bool AppDataTakeMutex(void) {
  return TakeMutex();
}

void AppDataFreeMutex(void) {
  FreeMutex();
}
