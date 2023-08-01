//==========================================================================
// Battery Management chip
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
#include "battery.h"

#include <string.h>
#include <unistd.h>

#include "app_utils.h"
#include "debug.h"
#include "driver/i2c.h"
#include "mx_target.h"
#include "packer.h"

//==========================================================================
// Define
//==========================================================================
#define I2C_ADDR_BQ27220 0x55

#define REG_CONTROL 0x00
#define REG_MAC 0x3e
#define REG_MAC_DATA 0x40
#define REG_MAC_DATASUM 0x60

//==========================================================================
// Variables
//==========================================================================
static BatteryInfo_t gBatteryInfo;

//==========================================================================
// Send Stop
//==========================================================================
static void SendStop(void) {
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  i2c_master_stop(i2c_link);
  i2c_master_cmd_begin(I2CNUM, i2c_link, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(i2c_link);
}

//==========================================================================
// Read registers
//==========================================================================
static bool ReadRegister(uint8_t aRegister, uint8_t *aData, uint16_t aLen) {
  bool ret = false;
  bool link_error = true;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  memset(aData, 0xff, aLen);
  for (;;) {
    // Write register address
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BQ27220 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, aRegister, true) != ESP_OK) {
      break;
    }

    // Read data
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BQ27220 << 1) | I2C_MASTER_READ, true) != ESP_OK) {
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
      printf("ERROR. Battery ReadRegister 0x%02X I2C transaction failed.\n", aRegister);
      break;
    }

    // all done
    ret = true;
    break;
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(1 / portTICK_PERIOD_MS);

  if (link_error) {
    printf("ERROR. Battery ReadRegister() 0x%02X I2C link error.\n", aRegister);
  }
  return ret;
}

//==========================================================================
// Write registers
//==========================================================================
static bool WriteRegister(uint8_t aRegister, uint8_t *aData, uint16_t aLen) {
  bool ret = false;
  bool link_error = false;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  for (;;) {
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_BQ27220 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
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
    ret = true;
    break;
  }
  if (link_error) {
    printf("ERROR. Battery WriteRegister() 0x%02X I2C link error.\n", aRegister);
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(1 / portTICK_PERIOD_MS);
  return ret;
}

//==========================================================================
// Read Data Memory
//==========================================================================
static bool ReadDataMemory(uint16_t aAddr, uint8_t *aDest, uint16_t aLen) {
  uint8_t addr[2];
  uint8_t buf[32];
  uint8_t cal_checksum;

  // Limit the length
  if (aLen > sizeof(buf)) aLen = sizeof(buf);

  //
  memset(aDest, 0xff, aLen);

  // buf[0] = 0;
  // buf[1] = 0;
  // if (!WriteRegister(REG_MAC_DATASUM, buf, 2)) {
  //   SendStop();
  //   return;
  // }

  // Read whole block
  addr[0] = (aAddr & 0xff);
  addr[1] = (aAddr >> 8) & 0xff;
  if (!WriteRegister(REG_MAC, addr, 2)) {
    SendStop();
    return false;
  }
  if (!ReadRegister(REG_MAC_DATA, buf, sizeof(buf))) {
    SendStop();
    return false;
  }

  cal_checksum = addr[0] + addr[1];
  for (int i = 0; i < sizeof(buf); i++) {
    cal_checksum += buf[i];
  }
  cal_checksum = 0xff - cal_checksum;

  // Read and check checksum
  uint8_t reg_checksum[2];
  if (!ReadRegister(REG_MAC_DATASUM, reg_checksum, sizeof(reg_checksum))) {
    SendStop();
    return false;
  }
  if (reg_checksum[0] != cal_checksum) {
    printf("ERROR. Battery ReadDataMemory() at 0x%04X Checksum error.\n", aAddr);
    return false;
  }

  //
  memcpy(aDest, buf, aLen);
  return true;
}

//==========================================================================
// Change Data Memory
//==========================================================================
static bool ChangeDataMemory(uint16_t aAddr, uint8_t *aSrc, uint16_t aLen) {
  uint8_t addr_buf[2];
  uint8_t data_buf[32];
  uint8_t subcommand[2];
  uint8_t checksum_buf[2];
  uint8_t buf[8];
  uint8_t cal_checksum;

  // Limit the length
  if (aLen > sizeof(data_buf)) aLen = sizeof(data_buf);

  // Read whole block
  PackU16Le(addr_buf, aAddr);
  if (!WriteRegister(REG_MAC, addr_buf, sizeof(addr_buf))) {
    SendStop();
    return false;
  }
  if (!ReadRegister(REG_MAC_DATA, data_buf, sizeof(data_buf))) {
    SendStop();
    return false;
  }
  // DEBUG_HEX2STRING("REG_MAC_DATA: ", data_buf, sizeof(data_buf));

  cal_checksum = addr_buf[0] + addr_buf[1];
  for (int i = 0; i < sizeof(data_buf); i++) {
    cal_checksum += data_buf[i];
  }
  cal_checksum = 0xff - cal_checksum;

  // Read and check checksum
  if (!ReadRegister(REG_MAC_DATASUM, checksum_buf, sizeof(checksum_buf))) {
    SendStop();
    return false;
  }
  // DEBUG_HEX2STRING("REG_MAC_DATASUM: ", checksum_buf, sizeof(checksum_buf));
  if (checksum_buf[0] != cal_checksum) {
    printf("ERROR. Battery ChangeDataMemory() at 0x%04X Checksum error.\n", aAddr);
    return false;
  }

  // Modify the block data and calculate new checksum
  memcpy(data_buf, aSrc, aLen);
  cal_checksum = addr_buf[0] + addr_buf[1];
  for (int i = 0; i < sizeof(data_buf); i++) {
    cal_checksum += data_buf[i];
  }
  cal_checksum = 0xff - cal_checksum;
  // DEBUG_PRINTLINE("new checksum: %02X", cal_checksum);
  checksum_buf[0] = cal_checksum;

  // ENTER_CFG_UPDATE
  PackU16Le(subcommand, 0x0090);
  if (!WriteRegister(REG_CONTROL, subcommand, sizeof(subcommand))) {
    SendStop();
    return false;
  }

  bool success = false;
  uint16_t op_status;

  for (int i = 15; i > 0; i--) {
    vTaskDelay(50 / portTICK_PERIOD_MS);
    if (!ReadRegister(0x3a, buf, 2)) {
      SendStop();
      break;
    }
    op_status = UnpackU16Le(buf);
    if ((op_status & 0x0400) != 0) {
      DEBUG_PRINTLINE("Battery enter config update");
      success = true;
      break;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
  if (!success) {
    printf("ERROR. Battery ChangeDataMemory() failed to enable config update.\n");
    return false;
  }

  // Write block
  // DEBUG_HEX2STRING("REG_MAC: ", addr_buf, sizeof(addr_buf));
  // DEBUG_HEX2STRING("REG_MAC_DATA: ", data_buf, sizeof(data_buf));
  // DEBUG_HEX2STRING("REG_MAC_DATASUM: ", checksum_buf, sizeof(checksum_buf));

  if (!WriteRegister(REG_MAC, addr_buf, sizeof(addr_buf))) {
    SendStop();
    return false;
  }
  if (!WriteRegister(REG_MAC_DATA, data_buf, sizeof(data_buf))) {
    SendStop();
    return false;
  }
  if (!WriteRegister(REG_MAC_DATASUM, checksum_buf, sizeof(checksum_buf))) {
    SendStop();
    return false;
  }

  // EXIT_CFG_UPDATE_REINIT
  PackU16Le(subcommand, 0x0091);
  if (!WriteRegister(REG_CONTROL, subcommand, sizeof(subcommand))) {
    SendStop();
    return false;
  }
  vTaskDelay(10 / portTICK_PERIOD_MS);
  if (!ReadRegister(0x3a, buf, 2)) {
    SendStop();
    return false;
  }
  op_status = UnpackU16Le(buf);
  if ((op_status & 0x0400) != 0) {
    printf("ERROR. Battery ChangeDataMemory() failed to exit config update.\n");
    return false;
  }
  DEBUG_PRINTLINE("Battery exit config update");

  return true;
}

//==========================================================================
// Init
//==========================================================================
void BatteryInit(void) {
  memset(&gBatteryInfo, 0, sizeof(BatteryInfo_t));

  gBatteryInfo.voltage = NAN;
  gBatteryInfo.current = NAN;
}

//==========================================================================
// Update Battery Info
//==========================================================================
void BatteryProcess(void) {
  uint8_t buf[8];

  ReadRegister(0x3a, buf, 2);
  uint16_t op_status = UnpackU16Le(buf);
  // DEBUG_PRINTLINE("Battery OperationStatus()=0x%04X", op_status);

  ReadRegister(0x0a, buf, 2);
  uint16_t battery_status = UnpackU16Le(buf);
  // DEBUG_PRINTLINE("Battery BatteryStatus()=0x%04X", battery_status);

  if (ReadRegister(0x08, buf, 2)) {
    uint16_t raw_voltage = UnpackU16Le(buf);
    gBatteryInfo.voltage = (float)raw_voltage / 1000;
    // DEBUG_PRINTLINE("Battery Voltage()=%.2fV (0x%04X)", gBatteryInfo.voltage, raw_voltage);
  } else {
    gBatteryInfo.voltage = NAN;
  }

  if (ReadRegister(0x0C, buf, 2)) {
    uint16_t raw_current = UnpackU16Le(buf);
    if ((raw_current & 0x8000) != 0) {
      uint16_t current = (raw_current ^ 0xffff) + 1;
      gBatteryInfo.current = -1 * (float)current / 1000;
    } else {
      gBatteryInfo.current = (float)raw_current / 1000;
    }
    // DEBUG_PRINTLINE("Battery Current()=%.2fA (0x%04X)", gBatteryInfo.current, raw_current);
  } else {
    gBatteryInfo.current = NAN;
  }

  // Capacity info
  if (ReadRegister(0x3c, buf, 2)) {
    gBatteryInfo.designCapacity = UnpackU16Le(buf);
    // DEBUG_PRINTLINE("Battery designCapacity=%dmAh (0x%04X)", gBatteryInfo.designCapacity, gBatteryInfo.designCapacity);
  } else {
    gBatteryInfo.designCapacity = 0;
  }
  if (ReadRegister(0x12, buf, 2)) {
    gBatteryInfo.fullChargeCapacity = UnpackU16Le(buf);
    // DEBUG_PRINTLINE("Battery fullChargeCapacity=%dmAh (0x%04X)", gBatteryInfo.fullChargeCapacity,
    // gBatteryInfo.fullChargeCapacity);
  } else {
    gBatteryInfo.fullChargeCapacity = 0;
  }
  if (ReadRegister(0x10, buf, 2)) {
    gBatteryInfo.remainingCapacity = UnpackU16Le(buf);
    // DEBUG_PRINTLINE("Battery remainingCapacity=%dmAh (0x%04X)", gBatteryInfo.remainingCapacity, gBatteryInfo.remainingCapacity);
  } else {
    gBatteryInfo.remainingCapacity = 0;
  }
  if (ReadRegister(0x2c, buf, 2)) {
    uint16_t state_of_charge = UnpackU16Le(buf);
    //    DEBUG_PRINTLINE("Battery StateOfCharge=%d (0x%04X)", state_of_charge, state_of_charge);
    if (state_of_charge > 100) state_of_charge = 100;
    gBatteryInfo.percentage = (float)state_of_charge;
  } else {
    gBatteryInfo.percentage = NAN;
  }

  //
  // ReadDataMemory(0x929f, buf, 2);
  // DEBUG_PRINTLINE("0x929f (Design Capacity): %02X %02X (%dmAh)", buf[0], buf[1], UnpackU16(buf));

  // ReadDataMemory(0x92A3, buf, 2);
  // DEBUG_PRINTLINE("0x92A3 (Design Voltage): %02X %02X (%dmV)", buf[0], buf[1], UnpackU16(buf));

  // ReadDataMemory(0x9217, buf, 2);
  // DEBUG_PRINTLINE("0x9217 (Sleep Current): %02X %02X (%dmA)", buf[0], buf[1], UnpackU16(buf));

  // ReadDataMemory(0x9206, buf, 2);
  // DEBUG_HEX2STRING("0x9206 (Operation Config A): ", buf, 2);

  // ReadDataMemory(0x9208, buf, 2);
  // DEBUG_HEX2STRING("0x9208 (Operation Config B): ", buf, 2);
}

//==========================================================================
// Get battery info
//==========================================================================
void BatteryGetInfo(BatteryInfo_t *aInfo) { memcpy(aInfo, &gBatteryInfo, sizeof(BatteryInfo_t)); }

//==========================================================================
// Set Design Capacity
//==========================================================================
bool BatterySetDesignValue(uint16_t aCapacity, float aVoltage) {
  bool ret = true;
  uint8_t buf[8];
  uint16_t voltage = (uint16_t)(aVoltage * 1000);

  if (aCapacity > 32767) aCapacity = 32767;
  if (voltage > 32767) voltage = 32767;
  
  PackU16(&buf[0], aCapacity);
  PackU16(&buf[2], aCapacity);
  if (!ChangeDataMemory(0x929d, buf, 4)) ret = false;

  PackU16(buf, voltage);
  if (!ChangeDataMemory(0x92A3, buf, 2)) ret = false;


  ReadDataMemory(0x929f, buf, 2);
  DEBUG_PRINTLINE("0x929f (Design Capacity): %02X %02X (%dmAh)", buf[0], buf[1], UnpackU16(buf));

  ReadDataMemory(0x92A3, buf, 2);
  DEBUG_PRINTLINE("0x92A3 (Design Voltage): %02X %02X (%dmV)", buf[0], buf[1], UnpackU16(buf));

  return ret;
}