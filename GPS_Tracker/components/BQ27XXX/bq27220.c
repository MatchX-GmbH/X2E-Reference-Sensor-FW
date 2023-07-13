#include "bq27220.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdbool.h>
#include "esp_log.h"

#define i2c_Buf dataSTR->i2c_Bufx
#define I2C_MASTER_NUM				I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TIMEOUT_MS		100000
#define BQ27220_SENSOR_ADDR			0x55
#define TAG "bq27220.c"

esp_err_t i2c_write(uint8_t add_8b, char *buf, uint8_t size, bool single) {
  return i2c_master_write_to_device(I2C_MASTER_NUM, add_8b >> 1, (uint8_t*) buf, size, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

esp_err_t i2c_read(uint8_t add_8b, char *buf, uint8_t size, bool single) {
  return i2c_master_read_from_device(I2C_MASTER_NUM, add_8b >> 1, (uint8_t*) buf, size, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

void default_init(BQ27220_TypeDef *dataSTR) {
  dataSTR->shunt_res = BQ_SHUNT_RESISTOR;
}

int new_battery_init(BQ27220_TypeDef *dataSTR) {
  return (0);
}

uint16_t get_sub_cmmd(BQ27220_TypeDef *dataSTR, uint16_t cmmd) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[2] = cmmd >> 8;
  i2c_Buf[1] = cmmd & 255;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(50 / portTICK_RATE_MS);  // needs large delay here
  ESP_LOGI(TAG, "sub-a: %02x %02x %02x %02x \r\n", i2c_Buf[0], i2c_Buf[1], i2c_Buf[2], i2c_Buf[3]);
  int i = 0;
  for (i = 0; i < 100; i++) {
    vTaskDelay(1 / portTICK_RATE_MS);
    i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
    i2c_read(BQ27220_ADDR + 1, i2c_Buf, 4, false);
    ESP_LOGI(TAG, "sub-b: %02x %02x %02x %02x \r\n", i2c_Buf[0], i2c_Buf[1], i2c_Buf[2], i2c_Buf[3]);
    if ((i2c_Buf[0] == 0xa5) && (i2c_Buf[1] == 0xff))
      break;
  }
  vTaskDelay(1 / portTICK_RATE_MS);
  if (i > 98)
    ESP_LOGI(TAG, "sub-b: ERROR \r\n");
  return (i2c_Buf[0] << 8) | i2c_Buf[1];
}

uint16_t get_sub_cmmd_s(BQ27220_TypeDef *dataSTR, uint16_t cmmd) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[2] = cmmd >> 8;
  i2c_Buf[1] = cmmd & 255;
  //ESP_LOGI(TAG,"sub-a: %02x %02x %02x \r\n", i2c_Buf[0], i2c_Buf[1], i2c_Buf[2]);
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  i2c_Buf[0] = BQ_MACDATA;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 2, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //ESP_LOGI(TAG,"sub-b: %04x \r\n", (i2c_Buf[0] << 8) | i2c_Buf[1]);
  return (i2c_Buf[0] << 8) | i2c_Buf[1];
}

uint16_t get_reg_2B(BQ27220_TypeDef *dataSTR, uint8_t reg) {
  i2c_Buf[0] = reg;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 2, false);
  //ESP_LOGI(TAG,"sub-b: %02x %02x %02x %02x \r\n", i2c_Buf[0], i2c_Buf[1], i2c_Buf[2], i2c_Buf[3]);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(2);
  return (i2c_Buf[1] << 8) | i2c_Buf[0];
}

void enter_cfg_update(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0x90;
  i2c_Buf[2] = 0x00;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
}

void exitCfgUpdateExit(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = BQ_EXIT_CFG_UPDATE & 255;
  i2c_Buf[2] = BQ_EXIT_CFG_UPDATE >> 8;
  ESP_LOGI(TAG, "exitCfg_cmmd: ->  ");
  for (int i = 0; i < 3; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
}

void exitCfgUpdateReInit(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = BQ_EXIT_CFG_UPDATE_REINIT & 255;
  i2c_Buf[2] = BQ_EXIT_CFG_UPDATE_REINIT >> 8;
  ESP_LOGI(TAG, "exitInit_cmmd: ->  ");
  for (int i = 0; i < 3; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
}

void reset(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = BQ_RESET & 255;
  i2c_Buf[2] = BQ_RESET >> 8;
  ESP_LOGI(TAG, "reset_cmmd: ->  ");
  for (int i = 0; i < 3; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
}

void useProfile_1(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = BQ_SET_PROFILE_1 >> 8;
  i2c_Buf[2] = BQ_SET_PROFILE_1 & 255;
  ;
  ESP_LOGI(TAG, "Profile_1_cmmd: ->  ");
  for (int i = 0; i < 3; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
}

void useProfile_2(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = BQ_SET_PROFILE_2 >> 8;
  i2c_Buf[2] = BQ_SET_PROFILE_2 & 255;
  ;
  ESP_LOGI(TAG, "Profile_2_cmmd: ->  ");
  for (int i = 0; i < 3; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(200 / portTICK_RATE_MS);
}

uint16_t get_cs_len(BQ27220_TypeDef *dataSTR, bool pf) {
  i2c_Buf[0] = BQ_MACDATASUM;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 1, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(5);
  uint16_t csl = i2c_Buf[0];

  i2c_Buf[0] = BQ_MACDATALEN;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 1, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(5);
  csl = (csl << 8) | i2c_Buf[0];
  if (pf)
    ESP_LOGI(TAG, "get_cs: %02x\r\n", csl >> 8);
  if (pf)
    ESP_LOGI(TAG, "get_ln: %02x\r\n", csl & 255);
  return (csl);
}

uint8_t calc_checksum_rx(BQ27220_TypeDef *dataSTR, int length) {
  uint8_t cs = 0;
  //ESP_LOGI(TAG,"c_csum_rx_len: %02x -> ", length);
  i2c_Buf[0] = BQ_SUB;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 34, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(5 / portTICK_RATE_MS);
  for (int i = 0; i < length + 2; i++) {
    cs += i2c_Buf[i];
    //ESP_LOGI(TAG,"b: %02x cs: %02x  ", i2c_Buf[i], cs);
  }
  cs = 255 - cs;
  ESP_LOGI(TAG, "cs_rx:%02x \r\n", cs);
  return (cs);
}

uint8_t calc_checksum_tx(BQ27220_TypeDef *dataSTR, int length) {
  uint8_t cs = 0;
  ESP_LOGI(TAG, "cs_tx_len: %02x ->    ", length);
  for (int i = 0; i < length + 2; i++) {
    cs += i2c_Buf[i + 1];
    //ESP_LOGI(TAG,"i2c: %02x cs: %02x   ", i2c_Buf[i + 1], cs);
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i + 1]);
  }
  cs = 255 - cs;
  ESP_LOGI(TAG, "\r\ncs_tx: %02x\r\n", cs);
  return (cs);
}

uint32_t get_data_32(BQ27220_TypeDef *dataSTR, uint16_t sub_cmmd, int length) {
  i2c_Buf[0] = BQ_SUB;
  i2c_Buf[2] = sub_cmmd >> 8;
  i2c_Buf[1] = sub_cmmd & 255;
  dataSTR->subReg = sub_cmmd;
  //ESP_LOGI(TAG,"dat-32a: %02x %02x %02x \r\n", i2c_Buf[0], i2c_Buf[1], i2c_Buf[2]);
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(3 / portTICK_RATE_MS);
  //vTaskDelay(3); //needs to be at least 2

  dataSTR->checksum = calc_checksum_rx(dataSTR, length);

  uint16_t cslen = get_cs_len(dataSTR, false);
  dataSTR->macSumReg = cslen >> 8;
  dataSTR->macLenReg = cslen & 255;
  ESP_LOGI(TAG, "MacSum: %02x ,MacLen: %02x \r\n", dataSTR->macSumReg, dataSTR->macLenReg);

  i2c_Buf[0] = BQ_MACDATA;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, length, false);
  vTaskDelay(5 / portTICK_RATE_MS);
  //vTaskDelay(5); //seems to work down to 1
  for (int i = 0; i < length; i++) {
    dataSTR->macData[i] = dataSTR->i2c_Bufx[i];
    ESP_LOGI(TAG, "%02x,", dataSTR->macData[i]);
  }
  ESP_LOGI(TAG, "\r\n");
  //ESP_LOGI(TAG," mdl: %02x,  mdcs: %02x,  ccs: %02x\r\n", dataSTR->macLenReg, dataSTR->macSumReg, dataSTR->checksum);
  return ((uint32_t) dataSTR->subReg);
}

//#define BQ_SHORT          1

void change_ram_1_2_4(BQ27220_TypeDef *dataSTR, uint16_t sub_cmmd, uint32_t value, int qty, bool pre) {
  if (pre) {
#ifndef BQ_SHORT
    ESP_LOGI(TAG, "ram124_a: %04x ->         ", sub_cmmd);
    get_data_32(dataSTR, sub_cmmd, 32);
#endif

    if (qty == 1) {
      dataSTR->macData[0] = value & 255;
#ifdef BQ_SHORT
            dataSTR->macData[1] = 0;
            dataSTR->macData[2] = 0;
            dataSTR->macData[3] = 0;
#endif
    } else if (qty == 2) {
      dataSTR->macData[0] = (value >> 8) & 255;
      dataSTR->macData[1] = value & 255;
#ifdef BQ_SHORT
            dataSTR->macData[2] = 0;
            dataSTR->macData[3] = 0;
#endif
    } else if (qty == 4) {
      dataSTR->macData[0] = (value >> 24) & 255;
      dataSTR->macData[1] = (value >> 16) & 255;
      dataSTR->macData[2] = (value >> 8) & 255;
      dataSTR->macData[3] = value & 255;
    } else {
      ESP_LOGI(TAG, "ram124_q_error\r\n");
      return;
    }
  }

  i2c_Buf[0] = BQ_SUB;
  i2c_Buf[1] = sub_cmmd >> 8;
  i2c_Buf[2] = sub_cmmd & 255;
  if (pre) {
    i2c_Buf[3] = dataSTR->macData[0];
    i2c_Buf[4] = dataSTR->macData[1];
    i2c_Buf[5] = dataSTR->macData[2];
    i2c_Buf[6] = dataSTR->macData[3];
  }
  ESP_LOGI(TAG, "ram124_cmmd: ->  ");
  int i = 0;
#ifdef BQ_SHORT
    for(i = 0; i < qty + 3; i++) ESP_LOGI(TAG,"%02x ",i2c_Buf[i]);
    ESP_LOGI(TAG,"\r\n");
    uint8_t x = calc_checksum_tx(dataSTR, qty);
    i2c_write(BQ27220_ADDR, i2c_Buf, qty + 3, false);
#else
  for (i = 0; i < 32; i++)
    i2c_Buf[i + 3] = dataSTR->macData[i];
  for (i = 0; i < 35; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  uint8_t x = calc_checksum_tx(dataSTR, 32);
  i2c_write(BQ27220_ADDR, i2c_Buf, 35, false);
#endif
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(5);

  i2c_Buf[0] = BQ_MACDATASUM;
  i2c_Buf[1] = x;
#ifndef BQ_SHORT
  i2c_Buf[1] -= 0x20;  //why is this???? !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if ((sub_cmmd >= 0x91e0) && (sub_cmmd < BQ_CONFIG_TAPER_CURR))
    i2c_Buf[1]--;
#endif
  ESP_LOGI(TAG, "ram124_cs:   ->  ");
  for (i = 0; i < 2; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 2, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(5);

  i2c_Buf[0] = BQ_MACDATALEN;
#ifdef BQ_SHORT
  i2c_Buf[1] = qty + 4;
#else
  i2c_Buf[1] = 36;
#endif
  ESP_LOGI(TAG, "ram124_len:  ->  ");
  for (i = 0; i < 2; i++)
    ESP_LOGI(TAG, "%02x ", i2c_Buf[i]);
  ESP_LOGI(TAG, "\r\n");
  i2c_write(BQ27220_ADDR, i2c_Buf, 2, false);
  vTaskDelay(5 / portTICK_RATE_MS);
  //vTaskDelay(200);

  get_cs_len(dataSTR, true);
  //ESP_LOGI(TAG,"\r\n");

#ifndef BQ_SHORT
  ESP_LOGI(TAG, "ram124_x: %04x ->         ", sub_cmmd);
  get_data_32(dataSTR, sub_cmmd, 32);
  ESP_LOGI(TAG, "\r\n");
#endif
}

uint16_t get_16(BQ27220_TypeDef *dataSTR, uint16_t cmmd) {
  get_sub_cmmd_s(dataSTR, cmmd);
  i2c_Buf[0] = BQ_MACDATA;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 2, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  return (i2c_Buf[0] << 8) | i2c_Buf[1];
}

uint8_t get_8(BQ27220_TypeDef *dataSTR, uint16_t cmmd) {
  //ESP_LOGI(TAG,"get_8: %04x\r\n", cmmd);
  get_sub_cmmd_s(dataSTR, cmmd);
  i2c_Buf[0] = BQ_MACDATA;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 1, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  return i2c_Buf[0];
}

void seal(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0x30;
  i2c_Buf[2] = 0x00;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(2000 / portTICK_RATE_MS);
  //vTaskDelay(5);
}

void unseal(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0x14;
  i2c_Buf[2] = 0x04;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(5 / portTICK_RATE_MS);
  //vTaskDelay(5);

  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0x72;
  i2c_Buf[2] = 0x36;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(2000 / portTICK_RATE_MS);
  //vTaskDelay(5);
}

void full_access(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0xff;
  i2c_Buf[2] = 0xff;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(5 / portTICK_RATE_MS);
  //vTaskDelay(5);

  i2c_Buf[0] = BQ_CNTL;
  i2c_Buf[1] = 0xff;
  i2c_Buf[2] = 0xff;
  i2c_write(BQ27220_ADDR, i2c_Buf, 3, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  vTaskDelay(5 / portTICK_RATE_MS);
  //vTaskDelay(5);
}

uint32_t get_dev_id(BQ27220_TypeDef *dataSTR) {
  uint16_t dat = get_sub_cmmd(dataSTR, BQ_DEVICE_NUMBER);
  ESP_LOGI(TAG, "dat-idq: %04x \r\n", dat);
  if (dat != 0xa5ff)
    return (dat);
  i2c_Buf[0] = BQ_SUB;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 4, false);
  uint32_t id = (i2c_Buf[0] << 24) | (i2c_Buf[1] << 16) | (i2c_Buf[2] << 8) | i2c_Buf[3];
  ESP_LOGI(TAG, "dat-idq: %08x \r\n", id);
  vTaskDelay(5 / portTICK_RATE_MS);
  return (id);
}

uint32_t get_fw_rev(BQ27220_TypeDef *dataSTR) {
  uint16_t dat = get_sub_cmmd(dataSTR, BQ_FW_VERSION);
  //ESP_LOGI(TAG,"dat-fwq: %04x \r\n", dat);
  if (dat != 0xa5ff)
    return (dat);
  i2c_Buf[0] = BQ_SUB;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 34, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  return (i2c_Buf[0] << 24) | (i2c_Buf[1] << 16) | (i2c_Buf[2] << 8) | i2c_Buf[3];
}

uint32_t get_hw_rev(BQ27220_TypeDef *dataSTR) {
  uint16_t dat = get_sub_cmmd(dataSTR, BQ_HW_VERSION);
  //ESP_LOGI(TAG,"dat-fwq: %04x \r\n", dat);
  if (dat != 0xa5ff)
    return (dat);
  i2c_Buf[0] = BQ_SUB;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 34, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  return (i2c_Buf[0] << 24) | (i2c_Buf[1] << 16) | (i2c_Buf[2] << 8) | i2c_Buf[3];
}

void set_ntc_as_sensor(BQ27220_TypeDef *dataSTR, bool ntc) {
  uint16_t res = get_16(dataSTR, BQ_CONFIG_OP_CONFIG_A);
  ESP_LOGI(TAG, "s_ntc: %04x ", res);

  if (!(ntc)) {
    ESP_LOGI(TAG, " N ");
    res &= ~BQ_BIT_OCA_TEMPS;
    res |= BQ_BIT_OCA_BIE;

  } else {
    ESP_LOGI(TAG, " L ");
    res &= ~BQ_BIT_OCA_BIE;
    res |= BQ_BIT_OCA_TEMPS;
  }

  ESP_LOGI(TAG, "new: %04x\r\n", res);
  change_ram_1_2_4(dataSTR, BQ_CONFIG_OP_CONFIG_A - 0x20, (uint32_t) res, 2, true);
}
/*
 void set_reg(BQ27220_TypeDef *dataSTR, uint16_t reg, uint16_t da, int byt)
 {
 uint16_t res = get_16(dataSTR, reg);
 change_ram_1_2_4(dataSTR, reg, (uint32_t)da, byt, true);
 }
 */

uint16_t get_OS_reg(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_OS;
  i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  i2c_read(BQ27220_ADDR + 1, i2c_Buf, 2, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  dataSTR->osReg = (i2c_Buf[1] << 8) | i2c_Buf[0];
  return (dataSTR->osReg);
}

int read_registers(BQ27220_TypeDef *dataSTR) {
  i2c_Buf[0] = BQ_CNTL;
  int result = i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  if (result)
    return (result + 0x10);
  result = i2c_read(BQ27220_ADDR + 1, i2c_Buf, 32, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  if (result)
    return (result + 0x18);
  //vTaskDelay(1);

  dataSTR->cntlReg = (i2c_Buf[BQ_CNTL - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_CNTL - BQ_CNTL];
  dataSTR->arReg = (i2c_Buf[BQ_AR - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_AR - BQ_CNTL];
  dataSTR->artteReg = (i2c_Buf[BQ_ARTTE - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_ARTTE - BQ_CNTL];
  dataSTR->tempReg = (i2c_Buf[BQ_TEMP - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_TEMP - BQ_CNTL];
  dataSTR->voltReg = (i2c_Buf[BQ_VOLT - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_VOLT - BQ_CNTL];
  dataSTR->flagsReg = (i2c_Buf[BQ_FLAGS - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_FLAGS - BQ_CNTL];
  dataSTR->currentReg = (i2c_Buf[BQ_CURRENT - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_CURRENT - BQ_CNTL];

  dataSTR->rmReg = (i2c_Buf[BQ_RM - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_RM - BQ_CNTL];
  dataSTR->fccReg = (i2c_Buf[BQ_FCC - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_FCC - BQ_CNTL];
  dataSTR->aiReg = (i2c_Buf[BQ_AI - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_AI - BQ_CNTL];
  dataSTR->tteReg = (i2c_Buf[BQ_TTE - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_TTE - BQ_CNTL];
  dataSTR->ttfReg = (i2c_Buf[BQ_TTF - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_TTF - BQ_CNTL];
  dataSTR->siReg = (i2c_Buf[BQ_SI - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_SI - BQ_CNTL];
  dataSTR->stteReg = (i2c_Buf[BQ_STTE - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_STTE - BQ_CNTL];
  dataSTR->mliReg = (i2c_Buf[BQ_MLI - BQ_CNTL + 1] << 8) | i2c_Buf[BQ_MLI - BQ_CNTL];

  i2c_Buf[0] = BQ_MLTTE;
  result = i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  //if(result) return(result + 0x20);
  result = i2c_read(BQ27220_ADDR + 1, i2c_Buf, 32, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(1);
  //if(result) return(result + 0x28);

  dataSTR->mltteReg = (i2c_Buf[BQ_MLTTE - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_MLTTE - BQ_MLTTE];
  dataSTR->rawccReg = (i2c_Buf[BQ_RCC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_RCC - BQ_MLTTE];
  dataSTR->apReg = (i2c_Buf[BQ_AP - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_AP - BQ_MLTTE];
  dataSTR->intTempReg = (i2c_Buf[BQ_INTTEMP - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_INTTEMP - BQ_MLTTE];
  dataSTR->cycReg = (i2c_Buf[BQ_CYC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_CYC - BQ_MLTTE];
  dataSTR->socReg = (i2c_Buf[BQ_SOC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_SOC - BQ_MLTTE];
  dataSTR->sohReg = (i2c_Buf[BQ_SOH - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_SOH - BQ_MLTTE];

  dataSTR->cvReg = (i2c_Buf[BQ_CV - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_CV - BQ_MLTTE];
  dataSTR->ccReg = (i2c_Buf[BQ_CC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_CC - BQ_MLTTE];
  dataSTR->btpdReg = (i2c_Buf[BQ_BTPD - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_BTPD - BQ_MLTTE];
  dataSTR->btpcReg = (i2c_Buf[BQ_BTPC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_BTPC - BQ_MLTTE];
  dataSTR->osReg = (i2c_Buf[BQ_OS - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_OS - BQ_MLTTE];
  dataSTR->dcReg = (i2c_Buf[BQ_DC - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_DC - BQ_MLTTE];
  dataSTR->subReg = (i2c_Buf[BQ_SUB - BQ_MLTTE + 1] << 8) | i2c_Buf[BQ_SUB - BQ_MLTTE];

  i2c_Buf[0] = BQ_MACDATA;
  result = i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  //if(result) return(result + 0x30);
  result = i2c_read(BQ27220_ADDR + 1, i2c_Buf, 32, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(1);
  //if(result) return(result + 0x38);

  for (int i = 0; i < 32; i++) {
    dataSTR->macData[i] = i2c_Buf[i];
  }

  i2c_Buf[0] = BQ_MACDATASUM;
  result = i2c_write(BQ27220_ADDR, i2c_Buf, 1, true);
  //if(result) return(result + 0x40);
  result = i2c_read(BQ27220_ADDR + 1, i2c_Buf, 32, false);
  vTaskDelay(1 / portTICK_RATE_MS);
  //vTaskDelay(1);
  //if(result) return(result + 0x48);

  dataSTR->macSumReg = (i2c_Buf[BQ_MACDATASUM - BQ_MACDATASUM + 1] << 8) | i2c_Buf[BQ_MACDATASUM - BQ_MACDATASUM];
  dataSTR->macLenReg = (i2c_Buf[BQ_MACDATALEN - BQ_MACDATASUM + 1] << 8) | i2c_Buf[BQ_MACDATALEN - BQ_MACDATASUM];
  dataSTR->anacReg = i2c_Buf[BQ_ANACNT - BQ_MACDATASUM];
  dataSTR->rawcReg = (i2c_Buf[BQ_RAWC - BQ_MACDATASUM + 1] << 8) | i2c_Buf[BQ_RAWC - BQ_MACDATASUM];
  dataSTR->rawvReg = (i2c_Buf[BQ_RAWV - BQ_MACDATASUM + 1] << 8) | i2c_Buf[BQ_RAWV - BQ_MACDATASUM];
  dataSTR->rawtReg = (i2c_Buf[BQ_RAWT - BQ_MACDATASUM + 1] << 8) | i2c_Buf[BQ_RAWT - BQ_MACDATASUM];

  return (0);
}
