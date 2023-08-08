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
#include "driver/spi_master.h"
#include "driver/i2c.h"

//==========================================================================
// Defines
//==========================================================================
#define SPIHOST SPI3_HOST

// IO for X2E Reference Sensor
#define SPI1_MISO GPIO_NUM_37
#define SPI1_MOSI GPIO_NUM_35
#define SPI1_SCK GPIO_NUM_36

#define I2C1_SDA GPIO_NUM_1
#define I2C1_CLK GPIO_NUM_2

#define AIR_INT GPIO_NUM_14
#define USR_BUTTON GPIO_NUM_0
#define GPS_PWR GPIO_NUM_38
#define ACCEL_INT GPIO_NUM_3

#define I2C_PORT I2C_NUM_0
#define I2C_FREQ 25000

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

  gpio_reset_pin(GPS_PWR);
  gpio_set_direction(GPS_PWR, GPIO_MODE_OUTPUT);

  gpio_reset_pin(USR_BUTTON);
  gpio_set_direction(USR_BUTTON, GPIO_MODE_INPUT);

  gpio_reset_pin(ACCEL_INT);
  gpio_set_direction(ACCEL_INT, GPIO_MODE_INPUT);
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
      .max_transfer_sz = 32 };
  esp_err_t ret = spi_bus_initialize(SPIHOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    PrintLine("SPI bus init failed.");
  }
  gpio_set_drive_capability(SPI1_SCK, GPIO_DRIVE_CAP_1);
  gpio_set_drive_capability(SPI1_MOSI, GPIO_DRIVE_CAP_1);
}

//==========================================================================
// Init I2C
//==========================================================================
static void InitI2c(void) {
  // Setup I2C bus
  i2c_config_t i2c = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C1_SDA,
      .scl_io_num = I2C1_CLK,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_FREQ };
  ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_IRAM));
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
const char* MxTargetName(void) {
  return kMxTargetName;
}
