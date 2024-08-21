//==========================================================================
// I2C1 devices
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
#include "i2c_port_1.h"

#include "driver/gpio.h"
#include "app_utils.h"

//==========================================================================
// Defines
//==========================================================================
#define I2C1_SDA GPIO_NUM_11
#define I2C1_SCL GPIO_NUM_10

//==========================================================================
// Init
//==========================================================================
int8_t I2cPort1Init(void) {
  gpio_hold_dis(I2C1_SCL);
  gpio_hold_dis(I2C1_SDA);
  gpio_reset_pin(I2C1_SCL);
  gpio_reset_pin(I2C1_SDA);

  // Setup I2C bus
  i2c_config_t i2c_cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C1_SDA,
      .scl_io_num = I2C1_SCL,
      .sda_pullup_en = true,
      .scl_pullup_en = true,
      .master.clk_speed = 200000,
  };

  if (i2c_param_config(I2C_PORT1_NUM, &i2c_cfg) != ESP_OK) {
    PrintLine("I2C1 bus config failed.");
    return -1;
  } else if (i2c_driver_install(I2C_PORT1_NUM, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_IRAM) != ESP_OK) {
    PrintLine("I2C1 master driver install failed.");
    return -1;
  }
  return 0;
}

