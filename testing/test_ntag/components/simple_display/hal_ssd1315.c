//==========================================================================
// HAL - SSD1315 OLED 128x64
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
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "display_hal.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "i2c_port_1.h"

//==========================================================================
// Defines
//==========================================================================
#define OLED_WIDHT 128
#define OLED_HIGHT 64
#define SSD1315_NUM_OF_PAGE (OLED_HIGHT / 8)

//
#define GPIO_OLED_nRES GPIO_NUM_13
#define I2C_ADDR_SSD1315 0x3C
#define I2C_NUM I2C_PORT1_NUM

// Commands
#define CMD_CHARGE_PUMP_SETTING 0x8D
#define CMD_SET_DISPLAY 0xAE
#define CMD_SET_ALL_ON 0xA4

#define CMD_SET_PAGE 0xB0
#define CMD_SET_LOWER_ADDR 0x00
#define CMD_SET_UPPER_ADDR 0x10

//==========================================================================
// Variables
//==========================================================================
static uint8_t gCommBuf[16];
static uint8_t gDisplayBuf[OLED_WIDHT * SSD1315_NUM_OF_PAGE];
static bool gPageNeedUpdate[SSD1315_NUM_OF_PAGE];

//==========================================================================
// I2C
//==========================================================================
static int8_t I2CWrite(const uint8_t *aData, uint32_t aLen) {
  int8_t ret = -1;
  bool link_error = false;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  for (;;) {
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_SSD1315 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write(i2c_link, aData, aLen, true) != ESP_OK) {
      break;
    }
    if (i2c_master_stop(i2c_link) != ESP_OK) {
      break;
    }
    link_error = false;
    if (i2c_master_cmd_begin(I2C_NUM, i2c_link, 200 / portTICK_PERIOD_MS) != ESP_OK) {
      printf("ERROR. SSD1315 I2CWrite I2C transaction failed.\n");
      break;
    }

    // all done
    ret = 0;
    break;
  }
  if (link_error) {
    printf("ERROR. SSD1315 I2CWrite I2C link error.\n");
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(0);
  return ret;
}

static int8_t WriteDisplayBuf(uint16_t aOffset, uint16_t aLen) {
  int8_t ret = -1;
  bool link_error = false;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  if ((aOffset + aLen) > sizeof(gDisplayBuf)) {
    printf("ERROR. SSD1315 WriteDisplayBuf invalid parameters.\n");
    return -1;
  }
  uint8_t *data = &gDisplayBuf[aOffset];

  for (;;) {
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_SSD1315 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, 0x40, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write(i2c_link, data, aLen, true) != ESP_OK) {
      break;
    }
    if (i2c_master_stop(i2c_link) != ESP_OK) {
      break;
    }
    link_error = false;
    if (i2c_master_cmd_begin(I2C_NUM, i2c_link, 200 / portTICK_PERIOD_MS) != ESP_OK) {
      printf("ERROR. SSD1315 WriteDisplayBuf I2C transaction failed.\n");
      break;
    }

    // all done
    ret = 0;
    break;
  }
  if (link_error) {
    printf("ERROR. SSD1315 WriteDisplayBuf I2C link error.\n");
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(0);
  return ret;
}

//==========================================================================
//==========================================================================
static void SetReset(bool aReset) {
  if (aReset) {
    gpio_set_level(GPIO_OLED_nRES, 0);
  } else {
    gpio_set_level(GPIO_OLED_nRES, 1);
  }
}

static void SetupChargePump(void) {
  gCommBuf[0] = 0x00;
  gCommBuf[1] = CMD_CHARGE_PUMP_SETTING;
  gCommBuf[2] = 0x14;
  I2CWrite(gCommBuf, 3);
}

static void SetDisplay(bool aOn) {
  gCommBuf[0] = 0x80;
  gCommBuf[1] = CMD_SET_DISPLAY;
  if (aOn) {
    gCommBuf[1] |= 0x01;
  }
  I2CWrite(gCommBuf, 2);
}

static void SetDataOffset(uint8_t aPage, uint8_t aAddress) {
  gCommBuf[0] = 0x80;
  gCommBuf[1] = (CMD_SET_PAGE | (aPage & 0x07));
  gCommBuf[2] = 0x80;
  gCommBuf[3] = (CMD_SET_LOWER_ADDR | (aAddress & 0x0f));
  gCommBuf[4] = 0x80;
  gCommBuf[5] = (CMD_SET_UPPER_ADDR | ((aAddress >> 4) & 0x07));
  I2CWrite(gCommBuf, 6);
}

//==========================================================================
// Init
//==========================================================================
int8_t DisplayHalInit(void) {
  gpio_reset_pin(GPIO_OLED_nRES);
  gpio_set_direction(GPIO_OLED_nRES, GPIO_MODE_OUTPUT);
  SetReset(true);
  vTaskDelay(1);
  SetReset(false);
  vTaskDelay(1);

  // Clear whole display
  DisplayHalFillAll(0);

  // Draw 4 dots at corners
  DisplayHalWrPixel(0, 7, 1);
  DisplayHalWrPixel(OLED_WIDHT - 1, 0, 1);
  DisplayHalWrPixel(0, OLED_HIGHT - 1, 1);
  DisplayHalWrPixel(OLED_WIDHT - 1, OLED_HIGHT - 1, 1);
  DisplayHalUpdate();

  // Turn on screen
  SetupChargePump();
  SetDisplay(true);
  return 0;
}

//==========================================================================
// Return the display size
//==========================================================================
uint16_t DisplayhalWidth(void) { return OLED_WIDHT; }

uint16_t DisplayhalHeight(void) { return OLED_HIGHT; }

//==========================================================================
// Clear whole display
//==========================================================================
void DisplayHalFillAll(uint32_t aColor) {
  if (aColor == 0) {
    memset(gDisplayBuf, 0x00, sizeof(gDisplayBuf));
  } else {
    memset(gDisplayBuf, 0xff, sizeof(gDisplayBuf));
  }
  for (int i = 0; i < SSD1315_NUM_OF_PAGE; i++) {
    gPageNeedUpdate[i] = true;
  }
}

//==========================================================================
// Write a pixel to buffer
//==========================================================================
void DisplayHalWrPixel(uint16_t aX, uint16_t aY, uint32_t aColor) {
  uint8_t page = aY / 8;
  uint16_t offset = (page * OLED_WIDHT) + aX;
  uint8_t shift = aY % 8;
  uint8_t mask = 0x1 << shift;
  uint16_t gray = 0;

  if (aColor == 0xffffff) {
    gray = 0xff;
  } else if (aColor == 0) {
    gray = 0;
  } else {
    // Convert Color to gary  0.3R+0.59G+0.11B
    uint32_t red = ((aColor >> 16) & 0xff) * 77;
    uint32_t green = ((aColor >> 8) & 0xff) * 151;
    uint32_t blue = ((aColor >> 0) & 0xff) * 28;
    gray = (red + green + blue) / 256;
    if (gray > 255) gray = 255;
  }

  if (gray > 127) {
    gDisplayBuf[offset] |= mask;
  } else {
    gDisplayBuf[offset] &= ~mask;
  }
  gPageNeedUpdate[page] = true;
}

//==========================================================================
// Read a pixel from buffer
//==========================================================================
uint32_t DisplayHalRdPixel(uint16_t aX, uint16_t aY) {
  uint16_t offset = (aY / 8) * OLED_WIDHT + aX;
  uint8_t shift = aY % 8;
  uint8_t mask = 0x1 << shift;

  if ((gDisplayBuf[offset] & mask) != 0) {
    return 0xffffff;
  } else {
    return 0;
  }
}

//==========================================================================
// Update buffer to real display
//==========================================================================
void DisplayHalUpdate(void) {
  for (uint8_t i = 0; i < (OLED_HIGHT / 8); i++) {
    if (gPageNeedUpdate[i]) {
      SetDataOffset(i, 0);
      WriteDisplayBuf(i * OLED_WIDHT, OLED_WIDHT);
      gPageNeedUpdate[i] = false;
    }
  }
}
