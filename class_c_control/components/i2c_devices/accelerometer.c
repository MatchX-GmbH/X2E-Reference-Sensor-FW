//==========================================================================
// Accelerometer (LIS2DE12)
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
#include "accelerometer.h"

#include <string.h>

#include "debug.h"
#include "driver/i2c.h"
#include "lis2de12_reg.h"

#define I2C_MASTER_TIMEOUT_MS 100000
#define LIS2DE12_SENSOR_ADDR 0x30

//==========================================================================
//==========================================================================

// Default configuration for lis2de12
#define LIS2DE12_CONFIG_DEFAULT()                                                                                                \
  {                                                                                                                              \
    .sdo_pu_disc = LIS2DE12_PULL_UP_DISCONNECT, .temp_enable = LIS2DE12_TEMP_DISABLE, .odr = LIS2DE12_ODR_400Hz,                 \
    .hpm_mode = LIS2DE12_NORMAL_WITH_RST, .hpcf_cutoff = LIS2DE12_LIGHT, .fds = LIS2DE12_DISABLE, .bdu_status = LIS2DE12_ENABLE, \
    .fs = LIS2DE12_2g, .fifo_enable = LIS2DE12_DISABLE                                                                           \
  }

// State enable
typedef enum {
  LIS2DE12_DISABLE = 0,
  LIS2DE12_ENABLE,
} lis2dh12_state_t;

//  LIS2DH12 Init structure definition.
typedef struct {
  lis2de12_sdo_pu_disc_t sdo_pu_disc; /*!< Disconnect SDO/SA0 pull-up  */
  lis2de12_temp_en_t temp_enable;     /*!< Temperature sensor enable  */
  lis2de12_odr_t odr;                 /*!< Data rate selection  */
  lis2de12_hpm_t hpm_mode;            /*!< High-pass filter mode selection  */
  lis2de12_hpcf_t hpcf_cutoff;        /*!< High-pass filter bandwidth  */
  lis2dh12_state_t fds;               /*!< Filtered data selection  */
  lis2dh12_state_t bdu_status;        /*!< Block data update  */
  lis2de12_fs_t fs;                   /*!< Full-scale selection  */
  lis2dh12_state_t fifo_enable;       /*!< FIFO enable  */
} lis2de12_config_t;

// Raw data
typedef struct {
  int16_t raw_acce_x;
  int16_t raw_acce_y;
  int16_t raw_acce_z;
} lis2de12_raw_acce_value_t;

//==========================================================================
// Variables
//==========================================================================
static stmdev_ctx_t gLis2de12Device;
static bool gLis2de12Ready = false;

//==========================================================================
// Platform functions
//==========================================================================
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len) {
  /* Write multiple command */
  reg |= 0x80;
  uint8_t buf[10];
  int ret;
  buf[0] = reg;

  if (len > 0) memcpy(&buf[1], bufp, len);

  ret = i2c_master_write_to_device(I2CNUM, LIS2DE12_SENSOR_ADDR >> 1, buf, (len + 1), I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

  if (ret != ESP_OK) {
    printf("ERROR. LISDE12 I2C write failed.");
    return -1;
  }
  return 0;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len) {
  /* Read multiple command */
  reg |= 0x80;
  int ret;

  ret =
      i2c_master_write_read_device(I2CNUM, LIS2DE12_SENSOR_ADDR >> 1, &reg, 1, bufp, len, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

  if (ret != ESP_OK) {
    printf("ERROR. LISDE12 I2C read failed.");
    return -1;
  }
  return 0;
}

static void platform_delay(uint32_t ms) { vTaskDelay(ms / portTICK_RATE_MS); }

//==========================================================================
// Init
//==========================================================================
int8_t AccelerometerInit(void) {
  gLis2de12Ready = false;
  gLis2de12Device.write_reg = platform_write;
  gLis2de12Device.read_reg = platform_read;
  gLis2de12Device.mdelay = platform_delay;

  // Set config
  lis2de12_config_t config;

  memcpy(&config, &((lis2de12_config_t)LIS2DE12_CONFIG_DEFAULT()), sizeof(lis2de12_config_t));

  config.fs = LIS2DE12_4g;
  config.odr = LIS2DE12_ODR_10Hz;
  config.fds = LIS2DE12_DISABLE;

  //
  DEBUG_PRINTLINE("lis2de12_Initialization");

  uint8_t lis2de12_id = 0;

  if (lis2de12_device_id_get(&gLis2de12Device, &lis2de12_id) < 0) {
    printf("ERROR. LIS2DE12 get device ID failed.\n");
    return -1;
  } else if (lis2de12_id != 0x33) {
    printf("ERROR. LIS2DE12 invalid device ID 0x%02X.\n", lis2de12_id);
    return -1;
  }

  if (lis2de12_pin_sdo_sa0_mode_set(&gLis2de12Device, config.sdo_pu_disc) < 0) {
    printf("ERROR. LIS2DE12 set SDO mode failed.\n");
    return -1;
  }

  if (lis2de12_temperature_meas_set(&gLis2de12Device, config.temp_enable) < 0) {
    printf("ERROR. LIS2DE12 set temp measure failed.\n");
    return -1;
  }

  if (lis2de12_data_rate_set(&gLis2de12Device, config.odr) < 0) {
    printf("ERROR. LIS2DE12 set data rate failed.\n");
    return -1;
  }

  if (lis2de12_high_pass_mode_set(&gLis2de12Device, config.hpm_mode) < 0) {
    printf("ERROR. LIS2DE12 set high pass mode failed.\n");
    return -1;
  }

  if (lis2de12_high_pass_on_outputs_set(&gLis2de12Device, config.fds) < 0) {
    printf("ERROR. LIS2DE12 set high pass outputs failed.\n");
    return -1;
  }

  if (lis2de12_high_pass_bandwidth_set(&gLis2de12Device, config.hpcf_cutoff) < 0) {
    printf("ERROR. LIS2DE12 set high pass bandwidth failed.\n");
    return -1;
  }

  if (lis2de12_block_data_update_set(&gLis2de12Device, config.bdu_status) < 0) {
    printf("ERROR. LIS2DE12 set block data failed.\n");
    return -1;
  }

  if (lis2de12_full_scale_set(&gLis2de12Device, config.fs) < 0) {
    printf("ERROR. LIS2DE12 set full scale failed.\n");
    return -1;
  }

  if (lis2de12_fifo_set(&gLis2de12Device, config.fifo_enable) < 0) {
    printf("ERROR. LIS2DE12 set FIFO failed.\n");
    return -1;
  }

  // All done
  gLis2de12Ready = true;
  return 0;
}

//==========================================================================
// Is Data Ready?
//==========================================================================
bool AccelerometerIsDataReady(void) {
  lis2de12_reg_t reg;

  if (!gLis2de12Ready) {
    printf("ERROR. LIS2DE12 chip not ready.\n");
    return false;
  }

  if (lis2de12_xl_data_ready_get(&gLis2de12Device, &reg.byte) < 0) {
    printf("ERROR. LIS2DE12 get data ready failed.\n");
    return false;
  }

  if (reg.byte) return true;

  return false;
}

//==========================================================================
// Get Result
//==========================================================================
int8_t AccelerometerGetResult(AccelerometerResult_t *aResult) {
  lis2de12_fs_t fs;
  lis2de12_raw_acce_value_t raw_acce_value;

  //
  aResult->valid = false;
  aResult->acce_x = NAN;
  aResult->acce_y = NAN;
  aResult->acce_z = NAN;

  //
  if (!gLis2de12Ready) {
    printf("ERROR. LIS2DE12 chip not ready.\n");
    return -1;
  }

  //
  if (lis2de12_full_scale_get(&gLis2de12Device, &fs) < 0) {
    printf("ERROR. LIS2DE12 get full scale failed.\n");
    return -1;
  }

  if (!AccelerometerIsDataReady()) {
    printf("ERROR. LIS2DE12 data not ready.\n");
    return -1;
  }

  // Read accelerometer data
  if (lis2de12_acceleration_raw_get(&gLis2de12Device, (int16_t *)(&raw_acce_value)) < 0) {
    printf("ERROR. LIS2DE12 get data failed.\n");
    return -1;
  }

  if (fs == LIS2DE12_2g) {
    aResult->acce_x = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_x) / 1000;
    aResult->acce_y = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_y) / 1000;
    aResult->acce_z = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_z) / 1000;
  } else if (fs == LIS2DE12_4g) {
    aResult->acce_x = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_x) / 1000;
    aResult->acce_y = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_y) / 1000;
    aResult->acce_z = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_z) / 1000;
  } else if (fs == LIS2DE12_8g) {
    aResult->acce_x = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_x) / 1000;
    aResult->acce_y = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_y) / 1000;
    aResult->acce_z = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_z) / 1000;
  } else if (fs == LIS2DE12_16g) {
    aResult->acce_x = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_x) / 1000;
    aResult->acce_y = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_y) / 1000;
    aResult->acce_z = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_z) / 1000;
  }

  //
  aResult->valid = true;
  return 0;
}
