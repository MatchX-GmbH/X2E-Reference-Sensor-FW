/*
 * i2c_lis2de12.h
 *
 *  Created on: 30 Jun 2023
 *      Author: moham_em3gyci
 */

#ifndef COMPONENTS_ACCEL_INCLUDE_LIS2DE12_H_
#define COMPONENTS_ACCEL_INCLUDE_LIS2DE12_H_

#include "lis2de12_reg.h"
#include "esp_err.h"
#include "driver/i2c.h"

/**
 * @brief Default configuration for lis2de12
 *
 */
#define LIS2DE12_CONFIG_DEFAULT()     	   	 			        	\
	{                                 		                  	\
            .sdo_pu_disc = LIS2DE12_PULL_UP_DISCONNECT,     \
            .temp_enable = LIS2DE12_TEMP_DISABLE,           \
            .odr = LIS2DE12_ODR_400Hz,                    	\
            .hpm_mode = LIS2DE12_NORMAL_WITH_RST,           \
            .hpcf_cutoff = LIS2DE12_LIGHT,				        	\
            .fds = LIS2DE12_DISABLE,                      	\
            .bdu_status = LIS2DE12_ENABLE,  		        		\
            .fs = LIS2DE12_2g,    						              \
            .fifo_enable = LIS2DE12_DISABLE,		        		\
            .Interfaces_Functions = NULL			          		\
        }

/**
 * @brief  State enable
 */
typedef enum {
  LIS2DE12_DISABLE = 0,
  LIS2DE12_ENABLE,
} lis2dh12_state_t;

/**
 * @brief  LIS2DH12 Init structure definition.
 */
typedef struct {
  lis2de12_sdo_pu_disc_t sdo_pu_disc; /*!< Disconnect SDO/SA0 pull-up  */
  lis2de12_temp_en_t temp_enable; /*!< Temperature sensor enable  */
  lis2de12_odr_t odr; /*!< Data rate selection  */
  lis2de12_hpm_t hpm_mode; /*!< High-pass filter mode selection  */
  lis2de12_hpcf_t hpcf_cutoff; /*!< High-pass filter bandwidth  */
  lis2dh12_state_t fds; /*!< Filtered data selection  */
  lis2dh12_state_t bdu_status; /*!< Block data update  */
  lis2de12_fs_t fs; /*!< Full-scale selection  */
  lis2dh12_state_t fifo_enable; /*!< FIFO enable  */
  stmdev_ctx_t *Interfaces_Functions;
} lis2de12_config_t;

typedef struct {
  int16_t raw_acce_x;
  int16_t raw_acce_y;
  int16_t raw_acce_z;
} lis2de12_raw_acce_value_t;

typedef struct {
  float acce_x;
  float acce_y;
  float acce_z;
} lis2de12_acce_value_t;

extern stmdev_ctx_t dev_ctx;

esp_err_t lis2de12_initialization(i2c_port_t i2c_port, lis2de12_config_t *lis2de12_config);
esp_err_t lis2de12_get_acce(lis2de12_acce_value_t *acce_value);
bool lis2de12_Is_data_ready();

#endif /* COMPONENTS_ACCEL_INCLUDE_LIS2DE12_H_ */
