//==========================================================================
// Test
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
#include "test.h"

#include "app_utils.h"
#include "ble_dfu.h"
#include "debug.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "i2c_port_1.h"
#include "led.h"
#include "mx_target.h"
#include "sleep.h"
#include "test_display.h"
#include "test_nfc.h"

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Constants
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
// Flush keys
//==========================================================================
static void FlushInput(void) {
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    int key_in = getc(stdin);
    if (key_in < 0) break;
  }
}

//==========================================================================
// Send Stop
//==========================================================================
static void SendI2CStop(void) {
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  i2c_master_stop(i2c_link);
  i2c_master_cmd_begin(I2C_PORT1_NUM, i2c_link, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(i2c_link);
}

//==========================================================================
// Scan I2C
//==========================================================================
static bool TestI2cAddr(uint8_t aAddr) {
  bool ret = false;
  uint8_t buf[1];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  for (;;) {
    // Construct the transaction
    if (i2c_master_start(cmd) != ESP_OK) {
      PrintLine("ERROR. i2c_master_start() failed.");
      break;
    }
    if (i2c_master_write_byte(cmd, (aAddr << 1) | I2C_MASTER_READ, true) != ESP_OK) {
      PrintLine("ERROR. i2c_master_write_byte() failed.");
      break;
    }
    if (i2c_master_read(cmd, buf, sizeof(buf), I2C_MASTER_LAST_NACK) != ESP_OK) {
      PrintLine("ERROR. i2c_master_write_byte() failed.");
      break;
    }
    if (i2c_master_stop(cmd) != ESP_OK) {
      PrintLine("ERROR. i2c_master_stop() failed.");
      break;
    }

    // Start the transaction
    if (i2c_master_cmd_begin(I2C_PORT1_NUM, cmd, 200 / portTICK_PERIOD_MS)) {
      //      PrintLine("ERROR. i2c_master_cmd_begin() failed.");
      break;
    }

    // all done
    ret = true;
    break;
  }
  i2c_cmd_link_delete(cmd);

  //
  return ret;
}

static void ScanI2C(void) {
  PrintLine("Scanning address 0x00 to 0x7f");
  for (uint8_t i = 0; i < 0x80; i++) {
    if (TestI2cAddr(i)) {
      const char* dev = "UNKNOWN";
      switch (i) {
        case 0x28:
          dev = "CLRC663 (ADR_2=0, ADR_1=0)";
          break;
        case 0x29:
          dev = "CLRC663 (ADR_2=0, ADR_1=1)";
          break;
        case 0x2A:
          dev = "CLRC663 (ADR_2=1, ADR_1=0)";
          break;
        case 0x2B:
          dev = "CLRC663 (ADR_2=1, ADR_1=1)";
          break;
        case 0x3C:
          dev = "SSD1315 (SA0=0)";
          break;
        case 0x3D:
          dev = "SSD1315 (SA0=1)";
          break;
      }
      PrintLine("Found device at 0x%02X, %s", i, dev);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Finished.");
}

//==========================================================================
// Start DFU
//==========================================================================
static void StartDfu(void) {
  if (!BleDfuServerIsStarted()) {
    if (BleDfuServerStart("X2E_DFU") >= 0) {
      PrintLine("INFO. BLE DFU started.");
    }
  } else {
    PrintLine("INFO. BLE DFU already started.");
  }
}

//==========================================================================
// Top level
//==========================================================================
void TestGo(void) {
  //
  if (IsWakeByReset()) {
    PrintLine("Boot from reset.");
  }

  //
  MxTargetInit();
  LedInit();
  I2cPort1Init();
  BleDfuServerInit();
  
  //
  for (;;) {
    // Show menu
    PrintLine("=================================");
    PrintLine("test_ntag");
    PrintLine("=================================");
    PrintLine("  0) Start DFU.");
    PrintLine("  1) Scan devices (I2C%d)", I2C_PORT1_NUM);
    PrintLine("  2) Test Display");
    PrintLine("  3) Validate NTAG");
    PrintLine("  4) Initialize NTAG");
    PrintLine("  5) Reset NTAG");

    // Wait key
    int key_in = 0;
    while (key_in == 0) {
      // Read input from console
      key_in = getc(stdin);
      if (key_in >= 0) {
        break;
      } else {
        key_in = 0;
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    PrintLine("");

    // Tests
    switch (key_in) {
      case '0':
        StartDfu();
        break;
      case '1':
        ScanI2C();
        break;
      case '2':
        TestDisplay();
        break;
      case '3':
        TestNfc_Validate();
        break;
      case '4':
        TestNfc_Initialize();
        break;
      case '5':
        TestNfc_Reset();
        break;
    }

    // Flush
    FlushInput();
  }
}