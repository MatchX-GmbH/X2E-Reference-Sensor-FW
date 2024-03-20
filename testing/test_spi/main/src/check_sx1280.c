//==========================================================================
// Check SX1280
//==========================================================================
#include "check_sx1280.h"

#include <string.h>

#include "app_utils.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "radio/radio.h"
#include "radio/sx-gpio.h"
#include "radio/sx1280-hal.h"


//==========================================================================
//==========================================================================
DioIrqHandler *gSx1280DioIrqHandler = NULL;


//==========================================================================
//==========================================================================
void CheckSx1280Init() {
    SX1280HalInit();
    SX1280HalIoIrqInit(NULL);
}

//==========================================================================
// Check Busy pin
//==========================================================================
int CheckSx1280Busy(void) {
  // RESET low, expect BUSY high
  gpio_set_level(SX1280_nRES, 0);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  if (gpio_get_level(SX1280_BUSY) != 1) {
    PrintLine("SX1280: Check BUSY Failed. Expected BUSY is high when RESET low.\n");
    gpio_set_level(SX1280_nRES, 1);
    return -1;
  }

  // RESET high, expect BUSY low
  gpio_set_level(SX1280_nRES, 1);
  vTaskDelay(50 / portTICK_PERIOD_MS);
  if (gpio_get_level(SX1280_BUSY) != 0) {
    PrintLine("SX1280: Check BUSY Failed. Expected BUSY is low when RESET high.\n");
    return -1;
  }

  //
  PrintLine("SX1280: Check BUSY success.");
  return 0;
}

//==========================================================================
// Check SX1280
//==========================================================================
int CheckSx1280(void) {
  bool sx1280_passed = true;
  uint8_t sx1280_buf[16];
  uint32_t tick_timeout;

  // Check power up status
  PrintLine("SX1280: Check status");
  tick_timeout = GetTick();
  for (;;) {
    SX1280HalReadCommand(0xc0, sx1280_buf, 1);
    if (SX1280IsError()) {
      PrintLine("SX1280: GetStatus Failed.");
      sx1280_passed = false;
      break;
    }
    uint8_t mode = (sx1280_buf[0] >> 5) & 0x7;
    uint8_t cmd_status = (sx1280_buf[0] >> 2) & 0x07;
    PrintLine("SX1280: CircuitMode=%d, CommandStatus=%d", mode, cmd_status);

    if (mode == 2) {
      // STDBY_RC
      PrintLine("SX1280: STDBY_RC mode OK.");
      break;
    }

    if (TickElapsed(tick_timeout) >= 1000) {
      PrintLine("SX1280: Wait STDBY_RC Timeout");
      sx1280_passed = false;
      break;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  if (!sx1280_passed) return -1;

  //
  PrintLine("SX1280: Wait for 2s...");
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // Switch to STDBY_XOSC
  PrintLine("SX1280: Switch to STDBY_XOSC");
  sx1280_buf[0] = 1;
  SX1280HalWriteCommand(0x80, sx1280_buf, 1);
  if (SX1280IsError()) {
    PrintLine("SX1280: SetStandby Failed.");
    return -1;
  }
  tick_timeout = GetTick();
  for (;;) {
    SX1280HalReadCommand(0xc0, sx1280_buf, 1);
    if (SX1280IsError()) {
      PrintLine("SX1280: GetStatus Failed.");
      sx1280_passed = false;
      break;
    }
    uint8_t mode = (sx1280_buf[0] >> 5) & 0x7;
    uint8_t cmd_status = (sx1280_buf[0] >> 2) & 0x07;
    PrintLine("SX1280: CircuitMode=%d, CommandStatus=%d", mode, cmd_status);

    if (mode == 3) {
      // STDBY_XOSC
      PrintLine("SX1280: STDBY_XOSC mode OK.");
      break;
    }

    if (TickElapsed(tick_timeout) >= 1000) {
      PrintLine("SX1280: Wait STDBY_XOSC Timeout");
      sx1280_passed = false;
      break;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  if (!sx1280_passed) return -1;

  // Read Default LoRa Sync Word
  SX1280HalReadRegisters(0x944, sx1280_buf, 2);
  if (SX1280IsError()) {
    PrintLine("SX1280: Read register failed.");
    return -1;
  }
  Hex2String("SX1280: LoRa Sync Word ", sx1280_buf, 2);
  if ((sx1280_buf[0] != 0x14) || (sx1280_buf[1] != 0x24)) {
    PrintLine("SX1280: Wrong default LoRa Sync Word.");
    return -1;
  }

  // Write Reg
  sx1280_buf[0] = 0x55;
  sx1280_buf[1] = 0xaa;
  SX1280HalWriteRegisters(0x944, sx1280_buf, 2);
  memset (sx1280_buf, 0xff, sizeof(sx1280_buf));
  SX1280HalReadRegisters(0x944, sx1280_buf, 2);
  if ((sx1280_buf[0] != 0x55) || (sx1280_buf[1] != 0xaa)) {
    PrintLine("SX1280: Write registers failed.");
    return -1;
  }

  // Test buffer
  memset(sx1280_buf, 0xaa, 16);
  SX1280HalWriteBuffer(0, sx1280_buf, 16);
  SX1280HalReadBuffer(0, sx1280_buf, 16);
  for (int i = 0; i < 16; i++) {
    if (sx1280_buf[i] != 0xaa) {
      PrintLine("SX1280: Fill 0xaa buffer failed.");
      Hex2String("SX1280: Read=", sx1280_buf, 16);
      return -1;
    }
  }
  memset(sx1280_buf, 0x55, 16);
  SX1280HalWriteBuffer(0, sx1280_buf, 16);
  SX1280HalReadBuffer(0, sx1280_buf, 16);
  for (int i = 0; i < 16; i++) {
    if (sx1280_buf[i] != 0x55) {
      PrintLine("SX1280: Fill 0x55 buffer failed.");
      Hex2String("SX1280: Read=", sx1280_buf, 16);
      return -1;
    }
  }
  for (int i = 0; i < 16; i++) {
    sx1280_buf[i] = i;
  }
  SX1280HalWriteBuffer(0, sx1280_buf, 16);
  memset(sx1280_buf, 0, sizeof(sx1280_buf));
  SX1280HalReadBuffer(0, sx1280_buf, 8);
  for (int i = 0; i < 8; i++) {
    if (sx1280_buf[i] != i) {
      PrintLine("SX1280: Read/Write buffer failed.");
      Hex2String("SX1280: Read 0=", sx1280_buf, 8);
      return -1;
    }
  }
  SX1280HalReadBuffer(8, sx1280_buf, 8);
  for (int i = 0; i < 8; i++) {
    if (sx1280_buf[i] != (i + 8)) {
      PrintLine("SX1280: Read/Write buffer failed.");
      Hex2String("SX1280: Read 8=", sx1280_buf, 8);
      return -1;
    }
  }

  PrintLine("SX1280: Test success.");
  return 0;
}