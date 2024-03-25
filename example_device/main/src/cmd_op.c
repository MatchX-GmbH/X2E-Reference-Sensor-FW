//==========================================================================
// Command Interface
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
#include "cmd_op.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_data.h"
#include "app_utils.h"
#include "debug.h"
#include "lora_compon.h"
#include "task_priority.h"

#define JSMN_HEADER
#include "jsmn.h"

//==========================================================================
// Command table
//==========================================================================
// Funcs prototypes
static void CmdQrcodeGet(const char *aCmd);
static void CmdQrcodeSet(const char *aCmd, const char *aData);
static void CmdIrebootSet(const char *aCmd, const char *aData);
static void CmdLoRaGet(const char *aCmd);

// defines
#define COMMAND_MAX_SIZE 16
typedef void(CommandSetFunc)(const char *, const char *);
typedef void(CommandGetFunc)(const char *);
typedef struct {
  char command[COMMAND_MAX_SIZE];
  CommandGetFunc *funcGet;
  CommandSetFunc *funcSet;
} CommandTable_t;

// tables
#define COMMAND_COUNT 3
const CommandTable_t kCommandTable[COMMAND_COUNT] = {
    {.command = "+CQRCODE", .funcGet = CmdQrcodeGet, .funcSet = CmdQrcodeSet},
    {.command = "+CLORA", .funcGet = CmdLoRaGet, .funcSet = NULL},
    {.command = "+IREBOOT", .funcGet = NULL, .funcSet = CmdIrebootSet},
};

static const char *kStrOk = "OK";
static const char *kStrError = "ERROR";

//==========================================================================
// Variables
//==========================================================================
static TaskHandle_t gCmdOpHandle = NULL;
static uint32_t gLineIdx;
static char gLineBuf[256];

//==========================================================================
// +CQRCODE
//==========================================================================
static void CmdQrcodeGet(const char *aCmd) {
  if (AppDataTakeMutex()) {
    strncpy(gLineBuf, AppSettingsGet()->qrCode, sizeof(gLineBuf));
    AppDataFreeMutex();
    PrintLine("%s:%s", aCmd, gLineBuf);
    PrintLine("%s", kStrOk);
  } else {
    PrintLine("%s. AppData Mutex failed.", kStrError);
  }
}

static void CmdQrcodeSet(const char *aCmd, const char *aData) {
  jsmn_parser j_parser;
  jsmntok_t j_tokens[16];

  jsmn_init(&j_parser);

  int j_ret = jsmn_parse(&j_parser, aData, strlen(aData), j_tokens, sizeof(j_tokens) / sizeof(jsmntok_t));
  if (j_ret < 0) {
    PrintLine("%s. QRCODE JSON parse error.", kStrError);
  } else if ((j_ret < 1) || (j_tokens[0].type != JSMN_OBJECT)) {
    PrintLine("%s. QRCODE invalid JSON format.", kStrError);
  } else {
    if (AppDataTakeMutex()) {
      strncpy(AppSettingsGet()->qrCode, aData, sizeof(AppSettingsGet()->qrCode));
      AppDataFreeMutex();

      if (AppSettingsSaveToStorage() == 0) {
        PrintLine("%s", kStrOk);
      }
    } else {
      PrintLine("%s. AppData Mutex failed.", kStrError);
    }
  }
}

//==========================================================================
// +CLORA
//==========================================================================
static void eui_to_string(char *aStr, size_t aStrSize, const uint8_t *aEui) {
  for (int i = 0; i < LORA_EUI_LENGTH; i++) {
    if ((i * 2 + 2) >= aStrSize) break;
    snprintf(&aStr[i * 2], 3, "%02x", aEui[i]);
  }
}

static void CmdLoRaGet(const char *aCmd) {
  LoRaSettings_t settings;
  char str_dev_eui[LORA_EUI_LENGTH * 2 + 1];
  char str_join_eui[LORA_EUI_LENGTH * 2 + 1];

  if (LoRaComponGetSettings(&settings) == 0) {
    if (settings.provisionDone) {
      eui_to_string(str_dev_eui, sizeof(str_dev_eui), settings.devEui);
      eui_to_string(str_join_eui, sizeof(str_dev_eui), settings.joinEui);
      snprintf(gLineBuf, sizeof(gLineBuf),
               "{"
               "\"devEui\":\"%s\",\"joinEui\":\"%s\""
               "}",
               str_dev_eui, str_join_eui);
      PrintLine("%s:%s", aCmd, gLineBuf);
      PrintLine("%s", kStrOk);
    } else {
      PrintLine("%s. Not provisioned.", kStrError);
    }
  } else {
    PrintLine("%s", kStrError);
  }
}

//==========================================================================
// +IREBOOT
//==========================================================================
void CmdIrebootSet(const char *aCmd, const char *aData) {
  bool do_reboot = false;
  if (*aData == '0') {
    do_reboot = true;
  } else if (*aData == '8') {
    AppDataResetToDefault();
    LoRaComponResetSettings();
    do_reboot = true;
  }

  if (do_reboot) {
    PrintLine("%s", kStrOk);
    PrintLine("INFO. Going to reboot...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart();
  }

  PrintLine("%s", kStrError);
}

//==========================================================================
//==========================================================================
static void CommandOpTask(void *param) {
  memset(gLineBuf, 0, sizeof(gLineBuf));
  gLineIdx = 0;

  // Start loop
  for (;;) {
    vTaskDelay(1);
    int key_in = getc(stdin);
    if (key_in == 0x0d) key_in = 0x0a;

    if (key_in <= 0) {
      // nothing got
      vTaskDelay(100 / portTICK_PERIOD_MS);
    } else if (key_in == 0x0a) {
      // End of Line

      // To upper case
      char cmd_buf[COMMAND_MAX_SIZE];
      for (int i = 0; i < (COMMAND_MAX_SIZE - 1); i++) {
        char ch = gLineBuf[i];
        if ((ch >= 'a') && (ch <= 'z')) {
          ch = ch - 'a' + 'A';
        }
        cmd_buf[i] = ch;
      }

      // Blank command
      if (strcmp(cmd_buf, "AT") == 0) {
        PrintLine("%s", kStrOk);
      } else {
        // Match with commmand table
        for (int i = 0; i < COMMAND_COUNT; i++) {
          const CommandTable_t *cmd = &kCommandTable[i];

          size_t cmd_len = strlen(cmd->command);
          if ((memcmp(cmd_buf, "AT", 2) == 0) && (memcmp(&cmd_buf[2], cmd->command, cmd_len) == 0)) {
            cmd_len += 2;  // move with AT
            char ch = gLineBuf[cmd_len];
            if (ch == '?') {
              // Get
              if (cmd->funcGet != NULL) {
                cmd->funcGet(cmd->command);
              }
              break;
            } else if (ch == '=') {
              // Set
              if (cmd->funcSet != NULL) {
                cmd->funcSet(cmd->command, &gLineBuf[cmd_len + 1]);
              }
              break;
            }
          }
          // End of table
          if (i == (COMMAND_COUNT - 1)) {
            PrintLine("%s", kStrError);
          }
        }
      }

      memset(gLineBuf, 0, sizeof(gLineBuf));
      gLineIdx = 0;
    } else if (gLineIdx < (sizeof(gLineBuf) - 1)) {
      gLineBuf[gLineIdx] = (key_in & 0xff);
      gLineIdx++;
    }
  }

  //
  PrintLine("CommandOpTask ended.");
  gCmdOpHandle = NULL;
  vTaskDelete(NULL);
}

//==========================================================================
// Init
//==========================================================================
int8_t CommandOpInit(void) {
  // Create task
  if (xTaskCreate(CommandOpTask, "CommandOpTask", 4096, NULL, TASK_PRIO_GENERAL, &gCmdOpHandle) != pdPASS) {
    PrintLine("ERROR. Failed to create CommandOpTask task.");
    return -1;
  } else {
    return 0;
  }
}