/*
 * i2c_lis2de12.c
 *
 *  Created on: 30 Jun 2023
 *      Author: moham_em3gyci
 */
#include "lis2de12.h"
#include <string.h>
#include "freertos/FreeRTOS.h"

#define TAG "i2c_lis2de12.c"
#include "esp_log.h"

#define I2C_MASTER_TIMEOUT_MS		100000
#define lis2de12_SENSOR_ADDR		0x30

i2c_port_t lis2de12_i2c_port;

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len);
static void platform_delay(uint32_t ms);

stmdev_ctx_t dev_ctx = {
    .write_reg = platform_write,
    .read_reg = platform_read,
    .mdelay = platform_delay };

esp_err_t lis2de12_initialization(i2c_port_t i2c_port, lis2de12_config_t *lis2de12_config) {
  /* Initialize mems driver interface */
  esp_err_t ret;

  lis2de12_i2c_port = i2c_port;
  lis2de12_config->Interfaces_Functions = &dev_ctx;

  if (lis2de12_config == NULL) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "lis2de12_Initialization");

  uint8_t lis2de12_ID;

  ret = lis2de12_device_id_get(&dev_ctx, &lis2de12_ID);
  if ((ret != 0) || (lis2de12_ID != 0x33)) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_pin_sdo_sa0_mode_set(&dev_ctx, lis2de12_config->sdo_pu_disc);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_temperature_meas_set(&dev_ctx, lis2de12_config->temp_enable);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_data_rate_set(&dev_ctx, lis2de12_config->odr);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_high_pass_mode_set(&dev_ctx, lis2de12_config->hpm_mode);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_high_pass_on_outputs_set(&dev_ctx, lis2de12_config->fds);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_high_pass_bandwidth_set(&dev_ctx, lis2de12_config->hpcf_cutoff);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_block_data_update_set(&dev_ctx, lis2de12_config->bdu_status);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_full_scale_set(&dev_ctx, lis2de12_config->fs);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  ret = lis2de12_fifo_set(&dev_ctx, lis2de12_config->fifo_enable);
  if (ret != 0) {
    ESP_LOGE(TAG, "lis2de12_Config_Error");
    return ESP_FAIL;
  }

  return ESP_OK;
}

bool lis2de12_Is_data_ready() {
  lis2de12_reg_t reg;
  esp_err_t ret = lis2de12_xl_data_ready_get(&dev_ctx, &reg.byte);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "lis2de12_Read_Error");
    return ESP_FAIL;
  }

  if (reg.byte)
    return true;

  return false;
}

esp_err_t lis2de12_get_acce(lis2de12_acce_value_t *acce_value) {
  lis2de12_fs_t fs;
  lis2de12_raw_acce_value_t raw_acce_value;
  esp_err_t ret = lis2de12_full_scale_get(&dev_ctx, &fs);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "lis2de12_Read_Error");
    return ESP_FAIL;
  }

//	lis2de12_reg_t reg;
//	/* Read output only if new value available */
//	ret = lis2de12_xl_data_ready_get(&dev_ctx, &reg.byte);
//
//	if (ret != ESP_OK) {
//		ESP_LOGE(TAG, "lis2de12_Read_Error");
//		return ESP_FAIL;
//	}

//	if (reg.byte) {
  /* Read accelerometer data */

  ret = lis2de12_acceleration_raw_get(&dev_ctx, (int16_t*) (&raw_acce_value));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "lis2de12_Read_Error");
    return ESP_FAIL;
  }

  if (fs == LIS2DE12_2g) {
    acce_value->acce_x = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_x);
    acce_value->acce_y = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_y);
    acce_value->acce_z = lis2de12_from_fs2_to_mg(raw_acce_value.raw_acce_z);
  } else if (fs == LIS2DE12_4g) {
    acce_value->acce_x = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_x);
    acce_value->acce_y = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_y);
    acce_value->acce_z = lis2de12_from_fs4_to_mg(raw_acce_value.raw_acce_z);
  } else if (fs == LIS2DE12_8g) {
    acce_value->acce_x = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_x);
    acce_value->acce_y = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_y);
    acce_value->acce_z = lis2de12_from_fs8_to_mg(raw_acce_value.raw_acce_z);
  } else if (fs == LIS2DE12_16g) {
    acce_value->acce_x = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_x);
    acce_value->acce_y = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_y);
    acce_value->acce_z = lis2de12_from_fs16_to_mg(raw_acce_value.raw_acce_z);
  }
//	}

  return ESP_OK;
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len) {

  /* Write multiple command */
  reg |= 0x80;
  uint8_t buf[10];
  int ret;
  buf[0] = reg;

  if (len > 0)
    memcpy(&buf[1], bufp, len);

  ret = i2c_master_write_to_device(lis2de12_i2c_port, lis2de12_SENSOR_ADDR >> 1, buf, (len + 1),
  I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "lis2de12_Write_Error");
    return ESP_FAIL;
  }
//  ESP_LOGI(TAG, "lis2de12_Write_OK");
  return ESP_OK;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len) {

  /* Read multiple command */
  reg |= 0x80;
  int ret;

  ret = i2c_master_write_read_device(lis2de12_i2c_port, lis2de12_SENSOR_ADDR >> 1, &reg, 1, bufp, len,
  I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "lis2de12_Read_Error = %d", ret);
    return ESP_FAIL;
  }
//  ESP_LOGI(TAG, "lis2de12_Read_OK");
  return ESP_OK;
}

/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
static void platform_delay(uint32_t ms) {
  vTaskDelay(ms / portTICK_RATE_MS);
}
