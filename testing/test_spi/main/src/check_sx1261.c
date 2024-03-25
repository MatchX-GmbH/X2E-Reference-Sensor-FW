//==========================================================================
// Check SX1261
//==========================================================================
#include <string.h>

#include "app_utils.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "radio/radio.h"
#include "radio/sx-gpio.h"
#include "radio/sx126x-hal.h"

//==========================================================================
//==========================================================================
DioIrqHandler *gSx126xDioIrqHandler = NULL;

//==========================================================================
// Init
//==========================================================================
void CheckSx1261Init(void) {
  SX126xIoInit();
  SX126xIoIrqInit(NULL);
}

//==========================================================================
// Check Busy pin
//==========================================================================
int CheckSx1261Busy(void) {
  // RESET low, expect BUSY high
  gpio_set_level(SX1261_nRES, 0);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  if (gpio_get_level(SX1261_BUSY) != 1) {
    PrintLine("SX1261: Check BUSY Failed. Expected BUSY is high when RESET low.\n");
    gpio_set_level(SX1261_nRES, 1);
    return -1;
  }

  // RESET high, expect BUSY low
  gpio_set_level(SX1261_nRES, 1);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  if (gpio_get_level(SX1261_BUSY) != 0) {
    PrintLine("SX1261: Check BUSY Failed. Expected BUSY is low when RESET high.\n");
    return -1;
  }

  //
  PrintLine("SX1261: Check BUSY success.");
  return 0;
}

//==========================================================================
// Check SX1261
//==========================================================================
int CheckSx1261(void) {
  bool sx1261_passed = true;
  uint8_t sx1261_buf[16];
  uint32_t tick_timeout;

  // Check power up status
  PrintLine("SX1261: Check status");
  tick_timeout = GetTick();
  for (;;) {
    SX126xReadCommand(0xc0, sx1261_buf, 1);
    if (SX126xIsError()) {
      PrintLine("SX1261: GetStatus Failed.");
      sx1261_passed = false;
      break;
    }
    uint8_t mode = (sx1261_buf[0] >> 4) & 0x7;
    uint8_t cmd_status = (sx1261_buf[0] >> 1) & 0x07;
    PrintLine("SX1261: CircuitMode=%d, CommandStatus=%d", mode, cmd_status);

    if (mode == 2) {
      // STDBY_RC
      PrintLine("SX1261: STDBY_RC mode OK.");
      break;
    }

    if (TickElapsed(tick_timeout) >= 1000) {
      PrintLine("SX1261: Wait STDBY_RC Timeout");
      sx1261_passed = false;
      break;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  if (!sx1261_passed) return -1;

  //
  PrintLine("SX1261: Wait for 2s...");
  vTaskDelay(2000 / portTICK_PERIOD_MS);


  // Switch to STDBY_XOSC
  PrintLine("SX1261: Switch to STDBY_XOSC");
  sx1261_buf[0] = 1;
  SX126xWriteCommand(0x80, sx1261_buf, 1);
  if (SX126xIsError()) {
    PrintLine("SX1261: SetStandby Failed.");
    return -1;
  }
  tick_timeout = GetTick();
  for (;;) {
    SX126xReadCommand(0xc0, sx1261_buf, 1);
    if (SX126xIsError()) {
      PrintLine("SX1261: GetStatus Failed.");
      sx1261_passed = false;
      break;
    }
    uint8_t mode = (sx1261_buf[0] >> 4) & 0x7;
    uint8_t cmd_status = (sx1261_buf[0] >> 1) & 0x07;
    PrintLine("SX1261: CircuitMode=%d, CommandStatus=%d", mode, cmd_status);

    if (mode == 3) {
      // STDBY_XOSC
      PrintLine("SX1261: STDBY_XOSC mode OK.");
      break;
    }

    if (TickElapsed(tick_timeout) >= 1000) {
      PrintLine("SX1261: Wait STDBY_XOSC Timeout");
      sx1261_passed = false;
      break;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  if (!sx1261_passed) return -1;

  // Read Default LoRa Sync Word
  SX126xReadRegisters(0x740, sx1261_buf, 2);
  if (SX126xIsError()) {
    PrintLine("SX1261: Read register failed.");
    return -1;
  }
  Hex2String("SX1261: LoRa Sync Word ", sx1261_buf, 2);
  if ((sx1261_buf[0] != 0x14) || (sx1261_buf[1] != 0x24)) {
    PrintLine("SX1261: Wrong default LoRa Sync Word.");
    return -1;
  }

  // Write Reg
  sx1261_buf[0] = 0x55;
  sx1261_buf[1] = 0xaa;
  SX126xWriteRegisters(0x740, sx1261_buf, 2);
  memset (sx1261_buf, 0xff, sizeof(sx1261_buf));
  SX126xReadRegisters(0x740, sx1261_buf, 2);
  if ((sx1261_buf[0] != 0x55) || (sx1261_buf[1] != 0xaa)) {
    PrintLine("SX1261: Write registers failed.");
    return -1;
  }

  // Test buffer
  memset(sx1261_buf, 0xaa, 16);
  SX126xWriteBuffer(0, sx1261_buf, 16);
  memset (sx1261_buf, 0xff, sizeof(sx1261_buf));
  SX126xReadBuffer(0, sx1261_buf, 16);
  for (int i = 0; i < 16; i++) {
    if (sx1261_buf[i] != 0xaa) {
      PrintLine("SX1261: Fill 0xaa buffer failed.");
      Hex2String("SX1261: Read=", sx1261_buf, 16);
      return -1;
    }
  }
  memset(sx1261_buf, 0x55, 16);
  SX126xWriteBuffer(0, sx1261_buf, 16);
  memset (sx1261_buf, 0xff, sizeof(sx1261_buf));
  SX126xReadBuffer(0, sx1261_buf, 16);
  for (int i = 0; i < 16; i++) {
    if (sx1261_buf[i] != 0x55) {
      PrintLine("SX1261: Fill 0x55 buffer failed.");
      Hex2String("SX1261: Read=", sx1261_buf, 16);
      return -1;
    }
  }
  for (int i = 0; i < 16; i++) {
    sx1261_buf[i] = i;
  }
  SX126xWriteBuffer(0, sx1261_buf, 16);
  memset(sx1261_buf, 0xff, sizeof(sx1261_buf));
  SX126xReadBuffer(0, sx1261_buf, 8);
  for (int i = 0; i < 8; i++) {
    if (sx1261_buf[i] != i) {
      PrintLine("SX1261: Read/Write buffer failed.");
      Hex2String("SX1261: Read 0=", sx1261_buf, 8);
      return -1;
    }
  }
  SX126xReadBuffer(8, sx1261_buf, 8);
  for (int i = 0; i < 8; i++) {
    if (sx1261_buf[i] != (i + 8)) {
      PrintLine("SX1261: Read/Write buffer failed.");
      Hex2String("SX1261: Read 8=", sx1261_buf, 8);
      return -1;
    }
  }

  //
  PrintLine("SX1261: Test success.");
  return 0;
}