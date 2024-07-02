//==========================================================================
// Board related functions
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
#include "mx_target.h"

#include "app_utils.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

//==========================================================================
// Defines
//==========================================================================
// IO fo Nexus
#define SPI1_MISO GPIO_NUM_37
#define SPI1_MOSI GPIO_NUM_38
#define SPI1_SCK GPIO_NUM_39

#define FLASH_CS GPIO_NUM_36

#define I2C_SDA GPIO_NUM_13
#define I2C_SCL GPIO_NUM_12

//==========================================================================
//==========================================================================
static const char *kMxTargetName = "NEXUS";

//==========================================================================
// Init GPIO
//==========================================================================
static void InitGpio(void) {
  gpio_reset_pin(FLASH_CS);
  gpio_set_direction(FLASH_CS, GPIO_MODE_OUTPUT);
  gpio_set_level(FLASH_CS, 1);
}

//==========================================================================
// Init SPI
//==========================================================================
static void InitSpi(void) {
  // Setup SPI bus
  spi_bus_config_t buscfg = {
      .mosi_io_num = SPI1_MOSI,
      .miso_io_num = SPI1_MISO,
      .sclk_io_num = SPI1_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 384,
  };
  if (spi_bus_initialize(SPIHOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
    PrintLine("SPI bus init failed.");
  }
  gpio_set_drive_capability(SPI1_SCK, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability(SPI1_MOSI, GPIO_DRIVE_CAP_1);
}

//==========================================================================
// Init I2C bus
//==========================================================================
static void InitI2c(void) {
  // Setup I2C bus
  i2c_config_t i2c_cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_SDA,
      .scl_io_num = I2C_SCL,
      .sda_pullup_en = GPIO_PULLUP_DISABLE,
      .scl_pullup_en = GPIO_PULLUP_DISABLE,
      // .slave.addr_10bit_en = 0,
      .master.clk_speed = 100000,
  };

  if (i2c_param_config(I2CNUM, &i2c_cfg) != ESP_OK) {
    PrintLine("I2C bus config failed.");
  } else if (i2c_driver_install(I2CNUM, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_IRAM) != ESP_OK) {
    PrintLine("I2C master driver install failed.");
  }
}

//==========================================================================
// Init of nexus board
//==========================================================================
void MxTargetInit(void) {
  InitGpio();
  InitSpi();
  InitI2c();
}

//==========================================================================
// Return the name of the target
//==========================================================================
const char *MxTargetName(void) { return kMxTargetName; }

//==========================================================================
// Prepare before sleep
//==========================================================================
void MxTargetPrepareForSleep(void) {
}

//==========================================================================
// Resume from Sleep
//==========================================================================
void MxTargetResumeFromSleep(void) {
}