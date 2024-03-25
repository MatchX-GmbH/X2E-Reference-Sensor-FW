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

//==========================================================================
// Defines
//==========================================================================
#define SPIHOST SPI3_HOST

// IO fo X2E Reference Sensor
#define SPI1_MISO GPIO_NUM_37
#define SPI1_MOSI GPIO_NUM_35
#define SPI1_SCK GPIO_NUM_36

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
  spi_bus_config_t buscfg = {
      .mosi_io_num = SPI1_MOSI,
      .miso_io_num = SPI1_MISO,
      .sclk_io_num = SPI1_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 32,
  };
  esp_err_t ret = spi_bus_initialize(SPIHOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    PrintLine("SPI bus init failed.");
  }
}

//==========================================================================
// Init of nexus board
//==========================================================================
void MxTargetInit(void) {
  InitGpio();
  InitSpi();
}

//==========================================================================
// Return the name of the target
//==========================================================================
const char *MxTargetName(void) {
  return kMxTargetName;
}