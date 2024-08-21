//==========================================================================
// Barometric Pressure Sensor (BME280)
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
#include "barometric_pressure.h"

#include <math.h>
#include <string.h>

#include "bme280.h"
#include "debug.h"
#include "driver/i2c.h"

//==========================================================================
//==========================================================================
#define I2C_ADDR_BME280 0x76

//==========================================================================
//==========================================================================
static struct bme280_dev gBme280Device;
static uint32_t gMeasDelay;
static bool gBme280Ready = false;

//==========================================================================
// BME280 device functions
//==========================================================================
static BME280_INTF_RET_TYPE ReadRegister(uint8_t aRegister, uint8_t *aData, uint32_t aLen, void *intf_ptr) {
  BME280_INTF_RET_TYPE ret = BME280_E_COMM_FAIL;
  bool link_error = true;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  memset(aData, 0xff, aLen);
  for (;;) {
    // Write register address
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BME280 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, aRegister, true) != ESP_OK) {
      break;
    }

    // Read data
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BME280 << 1) | I2C_MASTER_READ, true) != ESP_OK) {
      break;
    }

    if (i2c_master_read(i2c_link, aData, aLen, I2C_MASTER_LAST_NACK) != ESP_OK) {
      break;
    }

    // End
    if (i2c_master_stop(i2c_link) != ESP_OK) {
      break;
    }

    link_error = false;

    // Start the transaction
    if (i2c_master_cmd_begin(I2CNUM, i2c_link, 200 / portTICK_PERIOD_MS) != ESP_OK) {
      printf("ERROR. BME280 ReadRegister 0x%02X I2C transaction failed.\n", aRegister);
      break;
    }

    // all done
    ret = BME280_OK;
    break;
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(1 / portTICK_PERIOD_MS);

  if (link_error) {
    printf("ERROR. BME280 ReadRegister() 0x%02X I2C link error.\n", aRegister);
  }

  return ret;
}

static BME280_INTF_RET_TYPE WriteRegister(uint8_t aRegister, const uint8_t *aData, uint32_t aLen, void *intf_ptr) {
  BME280_INTF_RET_TYPE ret = BME280_E_COMM_FAIL;
  bool link_error = false;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  for (;;) {
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BME280 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }
    // Write register address
    if (i2c_master_write_byte(i2c_link, aRegister, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write(i2c_link, aData, aLen, true) != ESP_OK) {
      break;
    }
    if (i2c_master_stop(i2c_link) != ESP_OK) {
      break;
    }
    link_error = false;
    if (i2c_master_cmd_begin(I2CNUM, i2c_link, 200 / portTICK_PERIOD_MS) != ESP_OK) {
      printf("ERROR. Battery WriteRegister() 0x%02X I2C transaction failed.\n", aRegister);
      break;
    }

    // all done
    ret = BME280_OK;
    break;
  }
  if (link_error) {
    printf("ERROR. Battery WriteRegister() 0x%02X I2C link error.\n", aRegister);
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(1 / portTICK_PERIOD_MS);
  return ret;
}

static void Delayus(uint32_t period, void *intf_ptr) {
  // Convert to ms
  period = (period + 500) / 1000;
  if (period == 0) period = 1;
  vTaskDelay(period / portTICK_PERIOD_MS);
}

//==========================================================================
// Init
//==========================================================================
int8_t BarometricPressureInit(void) {
  gBme280Ready = false;
  gMeasDelay = 10000;
  memset(&gBme280Device, 0, sizeof(struct bme280_dev));
  gBme280Device.intf = BME280_I2C_INTF;
  gBme280Device.read = ReadRegister;
  gBme280Device.write = WriteRegister;
  gBme280Device.delay_us = Delayus;

  // Init
  if (bme280_init(&gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 init failed.\n");
    return -1;
  }

  // Setup for Weather monitoring
  struct bme280_settings settings;
  if (bme280_get_sensor_settings(&settings, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 get settings failed.\n");
    return -1;
  }

  settings.osr_h = BME280_OVERSAMPLING_1X;
  settings.osr_p = BME280_OVERSAMPLING_1X;
  settings.osr_t = BME280_OVERSAMPLING_1X;
  settings.filter = BME280_FILTER_COEFF_OFF;
  settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

  if (bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 set settings failed.\n");
    return -1;
  }

  // Set mode
  if (bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 set mode failed.\n");
    return -1;
  }

  //
  if (bme280_cal_meas_delay(&gMeasDelay, &settings) != BME280_OK) {
    printf("ERROR. BME280 calculate measurement delay failed.\n");
    return -1;
  }
  DEBUG_PRINTLINE("BME280 MeasDelay=%ums", gMeasDelay);

  // All done
  gBme280Ready = true;

  return 0;
}

//==========================================================================
// Start Measurement
//==========================================================================
int8_t BarometricPressureStartMeas(void) {
  if (!gBme280Ready) {
    printf("ERROR. BME280 is not ready.\n");
    return -1;
  }
  if (bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 set mode failed.\n");
    return -1;
  }

  return 0;
}

//==========================================================================
// Check measure is done
//==========================================================================
bool BarometricPressureIsMeasDone(void) {
  uint8_t status_reg;

  if (!gBme280Ready) {
    printf("ERROR. BME280 is not ready.\n");
    return false;
  }
  if (bme280_get_regs(BME280_REG_STATUS, &status_reg, 1, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 get status failed.\n");
    return false;
  }

  if ((status_reg & BME280_STATUS_MEAS_DONE) != 0) {
    return false;
  } else {
    return true;
  }
}

//==========================================================================
// Get result
//==========================================================================
int8_t BarometricPressureGetResult(BarometricPressureResult_t *aResult) {
  uint8_t status_reg;

  aResult->valid = false;
  aResult->pressure = NAN;
  aResult->humidity = NAN;
  aResult->temperature = NAN;

  if (!gBme280Ready) {
    printf("ERROR. BME280 is not ready.\n");
    return -1;
  }

  if (bme280_get_regs(BME280_REG_STATUS, &status_reg, 1, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 get status failed.\n");
    return -1;
  }

  if ((status_reg & BME280_STATUS_MEAS_DONE) != 0) {
    printf("ERROR. BME280 measurement still in progress.\n");
    return -1;
  }

  struct bme280_data comp_data;

  if (bme280_get_sensor_data(BME280_ALL, &comp_data, &gBme280Device) != BME280_OK) {
    printf("ERROR. BME280 get sensor data failed.\n");
    return -1;
  }

  // DEBUG_PRINTLINE("P=%.3fPa, T=%.3fC, H=%.3f%%RH", comp_data.pressure, comp_data.temperature, comp_data.humidity);
  aResult->valid = true;
  aResult->pressure = comp_data.pressure / 100;
  aResult->humidity = comp_data.humidity;
  aResult->temperature = comp_data.temperature;

  return 0;
}

//==========================================================================
// Get esterminted measurement time in ms
//==========================================================================
uint32_t BarometricPressureMeasTime(void) { return gMeasDelay; }