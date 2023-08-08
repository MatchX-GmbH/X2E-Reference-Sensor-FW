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
// IO fo X2E Reference Sensor
#define SPI1_MISO GPIO_NUM_37
#define SPI1_MOSI GPIO_NUM_35
#define SPI1_SCK GPIO_NUM_36

#define I2C_SDA GPIO_NUM_1
#define I2C_SCL GPIO_NUM_2

//==========================================================================
//==========================================================================
static const char *kMxTargetName = "X2E Ref Sensor";

//==========================================================================
// Init GPIO
//==========================================================================
static void InitGpio(void) {
  // gpio_reset_pin(FLASH_CS);
  // gpio_set_direction(FLASH_CS, GPIO_MODE_OUTPUT);
  // gpio_set_level(FLASH_CS, 1);
}

//==========================================================================
// Init SPI
//==========================================================================
static void InitSpi(void) {
  // Setup SPI bus
  spi_bus_config_t spi_cfg = {
      .mosi_io_num = SPI1_MOSI,
      .miso_io_num = SPI1_MISO,
      .sclk_io_num = SPI1_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 384,
  };
  if (spi_bus_initialize(SPIHOST, &spi_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
    PrintLine("SPI bus init failed.");
  }
  gpio_set_drive_capability(SPI1_SCK, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability(SPI1_MOSI, GPIO_DRIVE_CAP_1);
}

//==========================================================================
// Init I2C bus
//==========================================================================
static void InitI2c(void) {
  gpio_hold_dis(I2C_SCL);
  gpio_hold_dis(I2C_SDA);
  gpio_reset_pin(I2C_SCL);
  gpio_reset_pin(I2C_SDA);

  // Setup I2C bus
  i2c_config_t i2c_cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_SDA,
      .scl_io_num = I2C_SCL,
      .sda_pullup_en = true,
      .scl_pullup_en = true,
      .master.clk_speed = 25000,
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
  // Remove I2C
  i2c_driver_delete(I2CNUM);

#if defined (CONFIG_MATCHX_SLEEP_PULL_I2C_LOW)
  // Set I2C pin to low
  gpio_reset_pin(I2C_SCL);
  gpio_set_pull_mode(I2C_SCL, GPIO_PULLDOWN_ONLY);
  gpio_hold_en(I2C_SCL);
  gpio_reset_pin(I2C_SDA);
  gpio_set_pull_mode(I2C_SDA, GPIO_PULLDOWN_ONLY);
  gpio_hold_en(I2C_SDA);
#endif
}

//==========================================================================
// Resume from Sleep
//==========================================================================
void MxTargetResumeFromSleep(void) {
  InitI2c();
}