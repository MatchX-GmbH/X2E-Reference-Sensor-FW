//==========================================================================
// NFC Reader
//==========================================================================
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
// Naming conventions
// ~~~~~~~~~~~~~~~~~~
//                Class : Leading C
//               Struct : Leading T
//       typedef Struct : tailing _t
//         typedef enum : tailing _e
//             Constant : Leading k
//      Global Variable : Leading g
//    Function argument : Leading a
//       Local Variable : All lower case
//==========================================================================
#include "ntag.h"

#include "Board_X2E.h"
#include "app_utils.h"
#include "debug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "packer.h"
#include "phDriver.h"
#include "phNfcLib.h"
#include "phbalReg.h"
#include "task_priority.h"

//==========================================================================
// Defines
//==========================================================================
#define TASK_STACK_SIZE 0x20000

typedef struct {
  phbalReg_Type_t balParams;
  phNfcLib_AppContext_t appContext;
  void *palTop;
  phhalHw_Rc663_DataParams_t *pHal;
  phacDiscLoop_Sw_DataParams_t *pDiscLoop;
  phalMfNtag42XDna_Sw_DataParams_t *pNtag42X;
  phKeyStore_Sw_DataParams_t *pKeyStore;
  uint8_t responseHolder[64];
} NfcVar_t;

typedef struct {
  bool abort;
  bool inProgress;
} PollingParam_t;

typedef struct {
  uint8_t type;
  uint8_t option;
  uint8_t accessRead;
  uint8_t accessWrite;
  uint8_t accessReadWrite;
  uint8_t accessChange;
  uint32_t size;
} FileSettings_t;

typedef enum {
  RET_POLL_ERROR = -1,
  RET_POLL_NO_TAG = 0,
  RET_POLL_TAG_FOUND,
  RET_POLL_WRONG_TAG,
} PollTagRet_e;

// Key Store index
#define KEY_STORE_AUTH 0
#define KEY_STORE_OLD_KEY 1
#define KEY_STORE_NEW_KEY 2

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gPollingHandle = NULL;

static bool gLibInitDone;
static NfcVar_t gNfcVar;
static PollingParam_t gPollingParam;
static uint8_t gCommBuf[256];
static uint8_t gNtagKey[NUM_OF_NTAG_KEY][NTAG_KEY_SIZE];
static uint8_t gVerifyData[NTAG_VERIFY_DATA_SIZE];
static NtagActionResult_t gActionResult;
static NtagAction_e gAction;

//==========================================================================
// Constants
//==========================================================================
static uint8_t kDefaultKey[NTAG_KEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//==========================================================================
// Store key
//==========================================================================
static int8_t StoreKey(uint8_t aStoreIdx, const uint8_t *aKey) {
  uint8_t key[PH_CRYPTOSYM_AES128_KEY_SIZE];
  uint16_t key_type = 0;

  if (!gLibInitDone) {
    PrintLine("ERROR. NFC lib not init yet.");
    return -1;
  }
  memcpy(key, aKey, PH_CRYPTOSYM_AES128_KEY_SIZE);

  if (phKeyStore_Sw_FormatKeyEntry(gNfcVar.pKeyStore, aStoreIdx, PH_KEYSTORE_KEY_TYPE_AES128) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Format Key store %d failed.", aStoreIdx);
    return -1;
  }
  if (phKeyStore_SetKey(gNfcVar.pKeyStore, aStoreIdx, 0, PH_KEYSTORE_KEY_TYPE_AES128, key, PH_KEYSTORE_KEY_TYPE_AES128) !=
      PH_ERR_SUCCESS) {
    PrintLine("ERROR. Set Key store %d failed.", aStoreIdx);
    return -1;
  }

  if (phKeyStore_GetKey(gNfcVar.pKeyStore, aStoreIdx, 0, PH_CRYPTOSYM_AES128_KEY_SIZE, key, &key_type) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Get Key store %d failed.", aStoreIdx);
    return -1;
  }
  if ((memcmp(aKey, key, PH_CRYPTOSYM_AES128_KEY_SIZE) != 0) || (key_type != PH_KEYSTORE_KEY_TYPE_AES128)) {
    PrintLine("ERROR. Store key %d failed.", aStoreIdx);
    return -1;
  }

  return 0;
}

//==========================================================================
// Get file settings
//==========================================================================
static int8_t GetFileSettings(uint8_t aFileNumber, FileSettings_t *aSettings) {
  uint8_t comm_buf_len;

  memset(aSettings, 0, sizeof(FileSettings_t));

  //
  if ((aFileNumber < 0x01) || (aFileNumber > 0x03)) {
    PrintLine("ERROR. Get file %d settings failed. Invalid file number.", aFileNumber);
    return -1;
  }

  comm_buf_len = 0;
  if (phalMfNtag42XDna_GetFileSettings(gNfcVar.pNtag42X, aFileNumber, gCommBuf, &comm_buf_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Get file %d settings failed.", aFileNumber);
    return -1;
  }
  if (comm_buf_len < 7) {
    PrintLine("ERROR. Get file %d settings failed. Invalid resp len.", aFileNumber);
    return -1;
  }
  aSettings->type = gCommBuf[0];
  aSettings->option = gCommBuf[1];
  uint16_t access = UnpackU16Le(&gCommBuf[2]);
  aSettings->accessRead = (access >> 12) & 0x0f;
  aSettings->accessWrite = (access >> 8) & 0x0f;
  aSettings->accessReadWrite = (access >> 4) & 0x0f;
  aSettings->accessChange = (access >> 0) & 0x0f;

  aSettings->size = gCommBuf[4];
  aSettings->size |= (uint32_t)gCommBuf[5] << 8;
  aSettings->size |= (uint32_t)gCommBuf[6] << 16;

  DEBUG_PRINTLINE("File%d Settings: %02X, %X %X %X %X, %d", aFileNumber, aSettings->option, aSettings->accessRead,
                  aSettings->accessWrite, aSettings->accessReadWrite, aSettings->accessChange, aSettings->size);

  return 0;
}

//==========================================================================
// Change file settings
//==========================================================================
static int8_t ChangeFileSettings(uint8_t aFileNumber, const FileSettings_t *aSettings) {
  uint8_t access_rights[2];

  access_rights[0] = (aSettings->accessReadWrite << 4) | (aSettings->accessChange);
  access_rights[1] = (aSettings->accessRead << 4) | (aSettings->accessWrite);
  if (phalMfNtag42XDna_ChangeFileSettings(gNfcVar.pNtag42X,
                                          PHAL_MFNTAG42XDNA_SPECIFICS_ENABLED | PHAL_MFNTAG42XDNA_COMMUNICATION_ENC, aFileNumber,
                                          aSettings->option, access_rights, 0, NULL) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Change file %d settings failed.", aFileNumber);
    return -1;
  }

  return 0;
}

//==========================================================================
// Further check the NTAG
//==========================================================================
static int8_t CheckNtag(void) {
  uint16_t get_cfg_len;
  uint8_t *fci;
  uint16_t fci_len = 0;
  FileSettings_t file_settings;

  DEBUG_PRINTLINE("Check NTAG424DNA");
  //
  if (phalMfNtag42XDna_GetVersion(gNfcVar.pNtag42X, gCommBuf) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. phalMfNtag42XDna_GetVersion failed.");
    return -1;
  }
  if (phalMfNtag42XDna_GetConfig(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_ADDITIONAL_INFO, &get_cfg_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. phalMfNtag42XDna_GetConfig get version length failed.");
    return -1;
  }
  if (get_cfg_len > 0) {
    DEBUG_HEX2STRING("  GetVersion: ", gCommBuf, get_cfg_len);
  } else {
    PrintLine("ERROR. Get NTAG manufacturing data failed.");
    return -1;
  }
  if (get_cfg_len < 28) {
    DEBUG_PRINTLINE("  Version length too short. It is not a NTAG424NDA.");
    return -1;
  }
  if (gCommBuf[0] != 0x04) {
    DEBUG_PRINTLINE("  Incorrect Vendor ID. It is not a NTAG424NDA.");
    return -1;
  }
  if (gCommBuf[1] != 0x04) {
    DEBUG_PRINTLINE("  Incorrect HW type. It is not a NTAG424NDA.");
    return -1;
  }
  if (gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[0].bUidSize != 7) {
    DEBUG_PRINTLINE("  Incorrect UID length. It is not a NTAG424NDA.");
    return -1;
  }
  if (memcmp(&gCommBuf[14], gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[0].aUid, 7) != 0) {
    DEBUG_PRINTLINE("  Mismatch UID. It is not a NTAG424NDA or it using random UID.");
    return -1;
  }

  // Select DF
  uint8_t df_name[7] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
  if (phalMfNtag42XDna_IsoSelectFile(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_FCI_NOT_RETURNED, PHAL_MFNTAG42XDNA_SELECTOR_4, NULL,
                                     df_name, sizeof(df_name), 0x00, &fci, &fci_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Select DF failed.");
    return -1;
  }
  DEBUG_PRINTLINE("  Select DF success.");

  if (GetFileSettings(1, &file_settings) < 0) {
    return -1;
  }
  if (GetFileSettings(2, &file_settings) < 0) {
    return -1;
  }
  if (GetFileSettings(3, &file_settings) < 0) {
    return -1;
  }

  DEBUG_PRINTLINE("  It is a NTAG424NDA.");

  return 0;
}

//==========================================================================
// Authentication
//==========================================================================
static int8_t AuthWithKey(uint8_t aKeyIdx, bool aUseDefaultKey) {
  uint8_t key_store_idx = KEY_STORE_AUTH;
  uint8_t key_ver = 0;
  uint8_t pcd_cap_in[6];
  uint8_t pcd_cap[6];
  uint8_t pd_cap[6];
  memset(pcd_cap_in, 0, sizeof(pcd_cap));
  memset(pcd_cap, 0, sizeof(pcd_cap));
  memset(pd_cap, 0, sizeof(pd_cap));
  pcd_cap_in[0] = 0x02;

  // Set up key
  if (aKeyIdx >= NUM_OF_NTAG_KEY) {
    PrintLine("ERROR. Invalid key index.");
    return -1;
  }
  if (aUseDefaultKey) {
    if (StoreKey(key_store_idx, kDefaultKey) < 0) {
      return -1;
    }
  } else {
    if (StoreKey(key_store_idx, gNtagKey[aKeyIdx]) < 0) {
      return -1;
    }
  }

  //
  if (phalMfNtag42XDna_AuthenticateEv2(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_AUTHFIRST_NON_LRP, PHAL_MFNTAG42XDNA_NO_DIVERSIFICATION,
                                       key_store_idx, key_ver, aKeyIdx, NULL, 0, sizeof(pcd_cap_in), pcd_cap_in, pcd_cap,
                                       pd_cap) != PH_ERR_SUCCESS) {
    DEBUG_PRINTLINE("ERROR. PHAL_MFNTAG42XDNA_AUTHFIRST_NON_LRP failed.");
    return -1;
  }
  DEBUG_PRINTLINE("Authenticate First success. key%d", aKeyIdx);
  DEBUG_HEX2STRING("  PCD Caps: ", pcd_cap, sizeof(pcd_cap));
  DEBUG_HEX2STRING("   PD Caps: ", pd_cap, sizeof(pd_cap));

  if (phalMfNtag42XDna_AuthenticateEv2(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_AUTHNONFIRST_NON_LRP,
                                       PHAL_MFNTAG42XDNA_NO_DIVERSIFICATION, key_store_idx, key_ver, aKeyIdx, NULL, 0,
                                       sizeof(pcd_cap_in), pcd_cap_in, pcd_cap, pd_cap) != PH_ERR_SUCCESS) {
    DEBUG_PRINTLINE("ERROR. PHAL_MFNTAG42XDNA_AUTHNONFIRST_NON_LRP failed.");
    return -1;
  }
  DEBUG_PRINTLINE("Authenticate NonFirst success. key%d", aKeyIdx);
  return 0;
}

//==========================================================================
// Change key from default to gNtagKey
//==========================================================================
static int8_t UpdateKey(uint8_t aKeyIdx) {
  uint8_t key_ver = 0;

  // Set up key
  if (aKeyIdx >= NUM_OF_NTAG_KEY) {
    PrintLine("ERROR. Invalid key index.");
    return -1;
  }
  if (StoreKey(KEY_STORE_OLD_KEY, kDefaultKey) < 0) {
    return -1;
  }
  if (StoreKey(KEY_STORE_NEW_KEY, gNtagKey[aKeyIdx]) < 0) {
    return -1;
  }

  if (phalMfNtag42XDna_ChangeKey(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_CHGKEY_NO_DIVERSIFICATION, KEY_STORE_OLD_KEY, key_ver,
                                 KEY_STORE_NEW_KEY, key_ver, aKeyIdx, NULL, 0) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Update key %d failed.", aKeyIdx);
    return -1;
  }

  return 0;
}

//==========================================================================
// Change key from gNtagKey to default
//==========================================================================
static int8_t ResetKey(uint8_t aKeyIdx) {
  uint8_t key_ver = 0;

  // Set up key
  if (aKeyIdx >= NUM_OF_NTAG_KEY) {
    PrintLine("ERROR. Invalid key index.");
    return -1;
  }
  if (StoreKey(KEY_STORE_OLD_KEY, gNtagKey[aKeyIdx]) < 0) {
    return -1;
  }
  if (StoreKey(KEY_STORE_NEW_KEY, kDefaultKey) < 0) {
    return -1;
  }

  if (phalMfNtag42XDna_ChangeKey(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_CHGKEY_NO_DIVERSIFICATION, KEY_STORE_OLD_KEY, key_ver,
                                 KEY_STORE_NEW_KEY, key_ver, aKeyIdx, NULL, 0) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Reset key %d failed.", aKeyIdx);
    return -1;
  }

  return 0;
}

//==========================================================================
// Write NDEF record
//==========================================================================
static const char *kMoonchainUri = "geneva-explorer.moonchain.com/address/0x28479D68cD3ef5661BDB7505EFD1712D5D0951F6";
static const char *kTextRecord = "{\"T\":\"MxTag0\"}";

static int8_t WriteNdef(void) {
  uint8_t file_number = 2;
  uint8_t *ptr = gCommBuf;
  uint8_t *payload_start;
  uint8_t *payload_len;

  // Format NDEF
  ptr += PackU16(ptr, 0);  // Skip total length

  // URI record
  ptr += PackU8(ptr, 0x80 | 0x10 | 0x01);  // MessageBegin | ShortRecord | TNF=well-known type
  ptr += PackU8(ptr, 1);                   // Type length
  payload_len = ptr;
  ptr += PackU8(ptr, 0);    // Pack dummy len first
  ptr += PackU8(ptr, 'U');  // Type - URI record
  payload_start = ptr;
  ptr += PackU8(ptr, 0x04);                           // ID = https://
  memcpy(ptr, kMoonchainUri, strlen(kMoonchainUri));  // URI
  ptr += strlen(kMoonchainUri);
  PackU8(payload_len, ptr - payload_start);

  // Text record
  ptr += PackU8(ptr, 0x40 | 0x10 | 0x01);  // MessageEnd | ShortRecord | TNF=well-known type
  ptr += PackU8(ptr, 1);                   // Type length
  payload_len = ptr;
  ptr += PackU8(ptr, 0);    // Pack dummy len first
  ptr += PackU8(ptr, 'T');  // Type - Text
  payload_start = ptr;
  ptr += PackU8(ptr, 0x02);  // UTF8 | language code len is 2
  memcpy(ptr, "en", 2);      // IANA language code = en
  ptr += 2;
  memcpy(ptr, kTextRecord, strlen(kTextRecord));  // Text
  ptr += strlen(kTextRecord);
  PackU8(payload_len, ptr - payload_start);

  // Total len
  PackU16(gCommBuf, ptr - gCommBuf - 2);

  // Write
  uint8_t wr_offset[3] = {0, 0, 0};        // LSB first
  uint8_t wr_len[3] = {0x80, 0x00, 0x00};  // LSB first
  if (phalMfNtag42XDna_WriteData(gNfcVar.pNtag42X, PH_EXCHANGE_DEFAULT, 0x01, file_number, wr_offset, &gCommBuf[0], wr_len) !=
      PH_ERR_SUCCESS) {
    PrintLine("ERROR. Write NDEF Data 0 failed.");
    return -1;
  }
  wr_offset[0] = 128;
  if (phalMfNtag42XDna_WriteData(gNfcVar.pNtag42X, PH_EXCHANGE_DEFAULT, 0x01, file_number, wr_offset, &gCommBuf[128], wr_len) !=
      PH_ERR_SUCCESS) {
    PrintLine("ERROR. Write NDEF Data 1 failed.");
    return -1;
  }

  //
  return 0;
}

//==========================================================================
// Write Proprietary Data
//==========================================================================
static int8_t WriteProprietaryData(void) {
  uint8_t file_number = 3;

  uint8_t wr_offset[3] = {0, 0, 0};                         // LSB first
  uint8_t wr_len[3] = {NTAG_VERIFY_DATA_SIZE, 0x00, 0x00};  // LSB first
  memcpy(gCommBuf, gVerifyData, NTAG_VERIFY_DATA_SIZE);
  if (phalMfNtag42XDna_WriteData(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_COMMUNICATION_ENC, 0x01, file_number, wr_offset, gCommBuf,
                                 wr_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Write Proprietary Data failed.");
    return -1;
  }
  return 0;
}

//==========================================================================
// Check Proprietary Data
//==========================================================================
static int8_t CheckProprietaryData(void) {
  uint8_t file_number = 3;
  uint8_t *data;
  uint16_t data_len;
  uint8_t rd_offset[3] = {0, 0, 0};       // LSB first
  uint8_t rd_len[3] = {0x00, 0x0, 0x00};  // LSB first

  DEBUG_PRINTLINE("Check Proprietary Data");
  data = NULL;
  data_len = 0;
  if (phalMfNtag42XDna_ReadData(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_COMMUNICATION_ENC, 0x01, file_number, rd_offset, rd_len, &data,
                                &data_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Read Proprietary Data failed.");
    return -1;
  }
  if (data_len == 0) {
    PrintLine("ERROR. Read Proprietary Data failed. Return len = 0.");
    return -1;
  }
  if (memcmp(data, gVerifyData, NTAG_VERIFY_DATA_SIZE) != 0) {
    PrintLine("ERROR. Proprietary data verify failed.");
    return -1;
  }
  DEBUG_PRINTLINE("Proprietary Data verified.");

  return 0;
}

//==========================================================================
// Check Tamper
//==========================================================================
static int8_t CheckTamper(void) {
  uint8_t rx_len = 0;

  if (phalMfNtag42XDna_GetTagTamperStatus(gNfcVar.pNtag42X, gCommBuf, &rx_len) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Get Tamper status failed.");
    return -1;
  }
  if (rx_len != 2) {
    PrintLine("ERROR. Get Tamper status failed. Invalid resp len.");
    return -1;
  }
  if ((gCommBuf[0] == 0x43) && (gCommBuf[1] == 0x43)) {
    DEBUG_PRINTLINE("Tamper is close");
    return 0;
  } else {
    DEBUG_PRINTLINE("Tamper opened.");
    return -1;
  }
}

//==========================================================================
// Reset all file settings
//==========================================================================
static int8_t ResetFileSettings(void) {
  FileSettings_t file_settings;
  uint8_t file_number;

  // Change CC
  file_number = 1;
  file_settings.type = 0;
  file_settings.option = 0;
  file_settings.accessRead = 0xe;
  file_settings.accessWrite = 0x0;
  file_settings.accessReadWrite = 0x00;
  file_settings.accessChange = 0x0;
  if (ChangeFileSettings(file_number, &file_settings) < 0) {
    return -1;
  }

  // Change NDEF
  file_number = 2;
  file_settings.type = 0;
  file_settings.option = 0;
  file_settings.accessRead = 0xe;
  file_settings.accessWrite = 0xe;
  file_settings.accessReadWrite = 0x0e;
  file_settings.accessChange = 0x0;
  if (ChangeFileSettings(file_number, &file_settings) < 0) {
    return -1;
  }

  // Change Proprietary
  file_number = 3;
  file_settings.type = 0;
  file_settings.option = 3;
  file_settings.accessRead = 0x2;
  file_settings.accessWrite = 0x3;
  file_settings.accessReadWrite = 0x03;
  file_settings.accessChange = 0x0;
  if (ChangeFileSettings(file_number, &file_settings) < 0) {
    return -1;
  }

  //
  return 0;
}

//==========================================================================
// Finalize File settings
//==========================================================================
static int8_t FinalizeFileSettings(void) {
  FileSettings_t file_settings;
  uint8_t file_number;

  // Change NDEF
  file_number = 2;
  file_settings.type = 0;
  file_settings.option = 0;
  file_settings.accessRead = 0xe;
  file_settings.accessWrite = 0x1;
  file_settings.accessReadWrite = 0x01;
  file_settings.accessChange = 0x0;
  if (ChangeFileSettings(file_number, &file_settings) < 0) {
    return -1;
  }

  // Change Proprietary
  file_number = 3;
  file_settings.type = 0;
  file_settings.option = 3;
  file_settings.accessRead = 0x2;
  file_settings.accessWrite = 0x3;
  file_settings.accessReadWrite = 0x03;
  file_settings.accessChange = 0x0;
  if (ChangeFileSettings(file_number, &file_settings) < 0) {
    return -1;
  }

  return 0;
}

//==========================================================================
// Finalize NTAG configuration
//==========================================================================
static int8_t FinalizeConfiguration(void) {
  //
  gCommBuf[0] = 0x01;  // Enabel Tag Tamper
  gCommBuf[1] = 0x0E;  // free access to GetTTStatus
  if (phalMfNtag42XDna_SetConfiguration(gNfcVar.pNtag42X, PHAL_MFNTAG42XDNA_SET_CONFIG_OPTION7, gCommBuf, 2) != PH_ERR_SUCCESS) {
    PrintLine("ERROR. Enable Tag Tamper failed.");
    return -1;
  }

  return 0;
}

//==========================================================================
// Initialize the Ntag
//==========================================================================
static int8_t InitializeNtag(void) {
  //
  if (AuthWithKey(0, true) < 0) {
    PrintLine("ERROR. Auth with key 0 failed.");
    return -1;
  }
  if (ResetFileSettings() < 0) {
    return -1;
  }
  if (WriteNdef() < 0) {
    return -1;
  }

  //
  if (AuthWithKey(3, true) < 0) {
    PrintLine("ERROR. Auth with key 3 failed.");
    return -1;
  }
  if (WriteProprietaryData() < 0) {
    return -1;
  }
  if (CheckProprietaryData() < 0) {
    return -1;
  }

  //
  if (AuthWithKey(0, true) < 0) {
    PrintLine("ERROR. Auth with key 0 failed.");
    return -1;
  }
  if (FinalizeFileSettings() < 0) {
    return -1;
  }
  if (FinalizeConfiguration() < 0) {
    return -1;
  }
  if (UpdateKey(1) < 0) {
    return -1;
  }
  if (UpdateKey(2) < 0) {
    return -1;
  }
  if (UpdateKey(3) < 0) {
    return -1;
  }
  if (UpdateKey(0) < 0) {
    return -1;
  }

  return 0;
}

//==========================================================================
// Validate a NTag
//==========================================================================
static int8_t ValidateNTag(void) {
  if (AuthWithKey(2, false) < 0) {
    PrintLine("ERROR. Auth with key 2 failed.");
    return -1;
  }
  if (CheckProprietaryData() >= 0) {
    gActionResult.dataVerified = true;
  }

  if (CheckTamper() >= 0) {
    gActionResult.tamperClosed = true;
  }

  if ((gActionResult.dataVerified) && (gActionResult.tamperClosed)) {
    gActionResult.validated = true;
  }

  return 0;
}

//==========================================================================
// Reset a NTag to Default
//==========================================================================
static int8_t ResetNtag(void) {
  bool auth_with_default_key = false;

  if (AuthWithKey(0, false) < 0) {
    if (AuthWithKey(0, true) < 0) {
      PrintLine("ERROR. Auth with key 0 failed.");
      return -1;
    } else {
      auth_with_default_key = true;
    }
  }
  if (ResetFileSettings() < 0) {
    return -1;
  }
  if (!auth_with_default_key) {
    if (ResetKey(1) < 0) {
      return -1;
    }
    if (ResetKey(2) < 0) {
      return -1;
    }
    if (ResetKey(3) < 0) {
      return -1;
    }
    if (ResetKey(0) < 0) {
      return -1;
    }
  }

  return 0;
}

//==========================================================================
// Polling for a tag present
//==========================================================================
static PollTagRet_e PollTag(void) {
  phStatus_t status;
  uint16_t tags_detected = 0;
  uint16_t num_of_tags = 0;

  /* Set Discovery poll state to detection */
  status = phacDiscLoop_SetConfig(gNfcVar.pDiscLoop, PHAC_DISCLOOP_CONFIG_NEXT_POLL_STATE, PHAC_DISCLOOP_POLL_STATE_DETECTION);
  if (status != PH_ERR_SUCCESS) {
    PrintLine("ERROR. phacDiscLoop_SetConfig failed.");
    return RET_POLL_ERROR;
  }

  /* Switch off RF field */
  status = phhalHw_FieldOff(gNfcVar.pHal);
  if (status != PH_ERR_SUCCESS) {
    PrintLine("ERROR. phhalHw_FieldOff failed.");
    return RET_POLL_ERROR;
  }

  /* Wait for field-off time-out */
  status = phhalHw_Wait(gNfcVar.pHal, PHHAL_HW_TIME_MICROSECONDS, 5100);
  if (status != PH_ERR_SUCCESS) {
    PrintLine("ERROR. phhalHw_Wait failed.");
    return RET_POLL_ERROR;
  }

  /* Start discovery loop */
  // PrintLine("Start discovery loop.");
  status = phacDiscLoop_Run(gNfcVar.pDiscLoop, PHAC_DISCLOOP_ENTRY_POINT_POLL);
  // PrintLine("status=%X", status);

  if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_MULTI_TECH_DETECTED) {
    DEBUG_PRINTLINE("PHAC_DISCLOOP_MULTI_TECH_DETECTED");
  } else if (((status & PH_ERR_MASK) == PHAC_DISCLOOP_NO_TECH_DETECTED) ||
             ((status & PH_ERR_MASK) == PHAC_DISCLOOP_NO_DEVICE_RESOLVED)) {
  } else if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_EXTERNAL_RFON) {
    DEBUG_PRINTLINE("PHAC_DISCLOOP_EXTERNAL_RFON");
  } else if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_MULTI_DEVICES_RESOLVED) {
    DEBUG_PRINTLINE("PHAC_DISCLOOP_MULTI_DEVICES_RESOLVED");
  } else if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_DEVICE_ACTIVATED) {
    DEBUG_PRINTLINE("Card/Tag detected.");

    status = phacDiscLoop_GetConfig(gNfcVar.pDiscLoop, PHAC_DISCLOOP_CONFIG_NR_TAGS_FOUND, &num_of_tags);
    if (status != PH_ERR_SUCCESS) {
      PrintLine("ERROR. Get number of tags failed.");
      return RET_POLL_ERROR;
    }
    status = phacDiscLoop_GetConfig(gNfcVar.pDiscLoop, PHAC_DISCLOOP_CONFIG_TECH_DETECTED, &tags_detected);
    if (status != PH_ERR_SUCCESS) {
      DEBUG_PRINTLINE("Poll failed. Get tags tech failed.");
      return RET_POLL_ERROR;
    }
    DEBUG_PRINTLINE("  %d tags, tech=0x%X", num_of_tags, tags_detected);

    if (PHAC_DISCLOOP_CHECK_ANDMASK(tags_detected, PHAC_DISCLOOP_POS_BIT_MASK_A)) {
      DEBUG_PRINTLINE("  Technology : Type A");

      if (gNfcVar.pDiscLoop->sTypeATargetInfo.bT1TFlag) {
        DEBUG_PRINTLINE("  Type A : T1T detected \n");
      }

      for (uint16_t bIndex = 0; bIndex < num_of_tags; bIndex++) {
        uint8_t bTagType;

        DEBUG_PRINTLINE("  Card : %d", bIndex + 1);
        DEBUG_HEX2STRING("  UID: ", gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aUid,
                         gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].bUidSize);
        DEBUG_PRINTLINE("  SAK: 0x%x", gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak);
        if (gNfcVar.pDiscLoop->sTypeATargetInfo.sTypeA_I3P4.pAts != NULL) {
          DEBUG_HEX2STRING("  ATS: ", gNfcVar.pDiscLoop->sTypeATargetInfo.sTypeA_I3P4.pAts,
                           gNfcVar.pDiscLoop->sTypeATargetInfo.sTypeA_I3P4.pAts[0]);
        }

        if ((gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak & (uint8_t)~0xFB) == 0) {
          /* Bit b3 is set to zero, [Digital] 4.8.2 */
          /* Mask out all other bits except for b7 and b6 */
          bTagType = (gNfcVar.pDiscLoop->sTypeATargetInfo.aTypeA_I3P3[bIndex].aSak & 0x60);
          bTagType = bTagType >> 5;
          switch (bTagType) {
            case PHAC_DISCLOOP_TYPEA_TYPE2_TAG_CONFIG_MASK:
              DEBUG_PRINTLINE("  Type: Type 2 tag");
              return RET_POLL_WRONG_TAG;
            case PHAC_DISCLOOP_TYPEA_TYPE4A_TAG_CONFIG_MASK:
              DEBUG_PRINTLINE("  Type: Type 4A tag");
              return RET_POLL_TAG_FOUND;
            case PHAC_DISCLOOP_TYPEA_TYPE_NFC_DEP_TAG_CONFIG_MASK:
              DEBUG_PRINTLINE("  Type: P2P");
              return RET_POLL_WRONG_TAG;
            case PHAC_DISCLOOP_TYPEA_TYPE_NFC_DEP_TYPE4A_TAG_CONFIG_MASK:
              DEBUG_PRINTLINE("  Type: Type NFC_DEP and 4A tag");
              return RET_POLL_WRONG_TAG;
          }
        }
      }
    } else if (PHAC_DISCLOOP_CHECK_ANDMASK(tags_detected, PHAC_DISCLOOP_POS_BIT_MASK_B)) {
      DEBUG_PRINTLINE("Technology : Type B");
      return RET_POLL_WRONG_TAG;
    } else {
      DEBUG_PRINTLINE("Unsupported technology.");
      return RET_POLL_WRONG_TAG;
    }
  } else {
    if ((status & PH_ERR_MASK) == PHAC_DISCLOOP_FAILURE) {
      DEBUG_PRINTLINE("DiscLoop failure.");
    }
  }
  return RET_POLL_NO_TAG;
}

//==========================================================================
// NTAG handling task
//==========================================================================
static void NtagTask(void *aParam) {
  DEBUG_PRINTLINE("NtagTask start.");
  gPollingParam.abort = false;
  for (;;) {
    //
    if (gPollingParam.abort) {
      DEBUG_PRINTLINE("Polling aborted.");
      break;
    }
    if (!gLibInitDone) {
      PrintLine("ERROR. Polling failed. NFC Lib not init.");
      break;
    }
    PollTagRet_e poll_ret = PollTag();
    if (poll_ret == RET_POLL_ERROR) {
      break;
    } else if (poll_ret == RET_POLL_WRONG_TAG) {
      PrintLine("WARN. Invalid TAG.");
      break;
    } else if (poll_ret == RET_POLL_TAG_FOUND) {
      if (CheckNtag() < 0) {
        PrintLine("WARN. Invalid NTAG.");
      } else {
        PrintLine("INFO. NTAG detected.");

        switch (gAction) {
          case NTAG_ACTION_VALIDATE:
            if (ValidateNTag() < 0) {
              PrintLine("ERROR. Validate NTAG failed.");
            } else {
              PrintLine("INFO. Validate NTAG done.");
              gActionResult.actionSuccess = true;
            }
          break;
          case NTAG_ACTION_INITIALIZE:
          if (AuthWithKey(0, true) < 0) {
            PrintLine("ERROR. Initialize failed. Not a blank tag.");
          }
          else {
            if (InitializeNtag() < 0) {
              PrintLine("ERROR. Initialize tag failed.");
            } else {
              PrintLine("INFO. NTAG initialized.");
              gActionResult.actionSuccess = true;
            }
          }
          break;
          case NTAG_ACTION_RESET:
          if (ResetNtag() < 0) {
            PrintLine("ERROR. Reset NTAG failed.");
          } else {
            PrintLine("INFO. NTAG reset to default.");
            gActionResult.actionSuccess = true;
          }
          break;
        }
      }
      break;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  DEBUG_PRINTLINE("NtagTask exited.");
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gPollingParam.inProgress = false;
  vTaskDelete(NULL);
}

//==========================================================================
// Init
//==========================================================================
int8_t NtagInit(void) {
  // Init variables
  memset(&gPollingParam, 0, sizeof(PollingParam_t));
  memset(&gActionResult, 0, sizeof(NtagActionResult_t));
  memset(gNtagKey, 0, sizeof(gNtagKey));
  memset(gVerifyData, 0, sizeof(gVerifyData));

  // Init NFC Libraray
  if (!gLibInitDone) {
    memset(&gNfcVar, 0, sizeof(NfcVar_t));

    //
    DEBUG_PRINTLINE("Init OSAL");
    phOsal_Init();

    DEBUG_PRINTLINE("Init BAL");
    if (phbalReg_Init(&gNfcVar.balParams, sizeof(phbalReg_Type_t)) != PH_DRIVER_SUCCESS) {
      PrintLine("ERROR. NFC Lib init param failed.");
      return -1;
    }

    DEBUG_PRINTLINE("Init NFC Lib");
    gNfcVar.appContext.pBalDataparams = &gNfcVar.balParams;
    if (phNfcLib_SetContext(&gNfcVar.appContext) != PH_NFCLIB_STATUS_SUCCESS) {
      PrintLine("ERROR. NFC Lib set context failed.");
      return -1;
    }
    if (phNfcLib_Init() != PH_NFCLIB_STATUS_SUCCESS) {
      PrintLine("ERROR. NFC Lib init failed.");
      return -1;
    }
    gNfcVar.pHal = phNfcLib_GetDataParams(PH_COMP_HAL);

    DEBUG_PRINTLINE("Setup board IRQ.");
    BoardConfigureIrq(gNfcVar.pHal);

    gNfcVar.palTop = phNfcLib_GetDataParams(PH_COMP_AL_TOP);
    gNfcVar.pNtag42X = phNfcLib_GetDataParams(PH_COMP_AL_MFNTAG42XDNA);
    gNfcVar.pKeyStore = phNfcLib_GetDataParams(PH_COMP_KEYSTORE);

    DEBUG_PRINTLINE("Setup discovery loop.");
    gNfcVar.pDiscLoop = phNfcLib_GetDataParams(PH_COMP_AC_DISCLOOP);
    gNfcVar.pDiscLoop->sTypeATargetInfo.sTypeA_P2P.pAtrRes = gNfcVar.responseHolder;
    gNfcVar.pDiscLoop->sTypeATargetInfo.sTypeA_I3P4.pAts = gNfcVar.responseHolder;

    gLibInitDone = true;
    DEBUG_PRINTLINE("NFC lib init done.");
  }

  DEBUG_PRINTLINE("Key storage: %d", gNfcVar.pKeyStore->wNoOfKeyEntries);

  return 0;
}

//==========================================================================
// Set Key
//==========================================================================
int8_t NtagSetKey(uint8_t aIdx, const uint8_t *aKey, uint16_t aKeyLen) {
  if (!gLibInitDone) {
    PrintLine("ERROR. NFC lib not init yet.");
    return -1;
  }
  if (aIdx >= NUM_OF_NTAG_KEY) {
    PrintLine("ERROR. Invalid key index.");
    return -1;
  }
  if (aKeyLen < PH_CRYPTOSYM_AES128_KEY_SIZE) {
    PrintLine("ERROR. Invalid key length.");
    return -1;
  }
  memcpy(gNtagKey[aIdx], aKey, PH_CRYPTOSYM_AES128_KEY_SIZE);

  return 0;
}

//==========================================================================
// Set Verify data
//==========================================================================
int8_t NtagSetVerifyData(const uint8_t *aData, uint16_t aLen) {
  if (!gLibInitDone) {
    PrintLine("ERROR. NFC lib not init yet.");
    return -1;
  }
  if (aLen > NTAG_VERIFY_DATA_SIZE) {
    PrintLine("ERROR. Invalid verify data len.");
    return -1;
  }
  memset(gVerifyData, 0, sizeof(gVerifyData));
  memcpy(gVerifyData, aData, aLen);
  return 0;
}

//==========================================================================
// Wait NTag
//    Blocked until a Tag detected or timeout reach
//==========================================================================
int8_t NtagWait(NtagAction_e aAction, uint32_t aTimeToWait) {
  if (!gLibInitDone) {
    PrintLine("ERROR. NFC lib not init yet.");
    return -1;
  } else if (gPollingParam.inProgress) {
    PrintLine("ERROR. Polling still in progress.");
    return -1;
  } else {
    gPollingParam.inProgress = true;
    memset (&gActionResult, 0, sizeof(NtagActionResult_t));
    gAction = aAction;
    if (xTaskCreate(NtagTask, "NtagTask", TASK_STACK_SIZE, NULL, TASK_PRIO_GENERAL, &gPollingHandle) != pdPASS) {
      PrintLine("ERROR. Create NtagTask failed.");
      return -1;
    }

    //
    uint32_t tick_wait = GetTick();
    uint32_t time_wait_abort = 0;
    for (;;) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      if (aTimeToWait > 0) {
        if (TickElapsed(tick_wait) >= aTimeToWait) {
          // Time reach.
          gPollingParam.abort = true;
          PrintLine("WARN. Timeout. No NTAG detected.");
          aTimeToWait = 0;
          time_wait_abort = 7500;
          tick_wait = GetTick();
        }
      }
      if (time_wait_abort > 0) {
        if (TickElapsed(tick_wait) >= time_wait_abort) {
          PrintLine("ERROR. Abort polling taks failed.");
          vTaskDelete(gPollingHandle);
          gPollingHandle = NULL;
          tick_wait = GetTick();
        }
      }

      // Check task exited.
      eTaskState state = eTaskGetState(gPollingHandle);
      if (state == eDeleted) {
        break;
      }
    }
    return 0;
  }
}

//==========================================================================
// Get result of last action
//==========================================================================
NtagActionResult_t *NtagGetResult(void) {
  return &gActionResult;
}
