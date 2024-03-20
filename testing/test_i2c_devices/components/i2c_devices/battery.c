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
#include <math.h>

#include "debug.h"
#include "driver/i2c.h"
#include "mx_target.h"

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


//==========================================================================
// Helper
//==========================================================================
static uint16_t UnpackU16Le(const uint8_t *aSrc) {
  uint16_t ret;
  const uint8_t *ptr = aSrc;

  ret = (uint16_t)ptr[1];
  ret <<= 8;
  ret |= (uint16_t)ptr[0];

  return ret;
}

static uint16_t PackU16Le(uint8_t *aDest, uint16_t aValue) {
  aDest[1] = (uint8_t)(aValue >> 8);
  aDest[0] = (uint8_t)(aValue);

  return 2;
}

static uint16_t PackU16(uint8_t *aDest, uint16_t aValue) {
  aDest[0] = (uint8_t)(aValue >> 8);
  aDest[1] = (uint8_t)(aValue);

  return 2;
}

static uint16_t UnpackU16(const uint8_t *aSrc) {
  uint16_t ret;
  const uint8_t *ptr = aSrc;

  ret = *ptr;
  ret <<= 8;
  ptr++;
  ret |= *ptr;

  return ret;
}

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
static int8_t ReadRegister(uint8_t aRegister, uint8_t *aData, uint16_t aLen) {
  int8_t ret = -1;
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
    ret = 0;
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
static int8_t WriteRegister(uint8_t aRegister, uint8_t *aData, uint16_t aLen) {
  int8_t ret = -1;
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
    ret = 0;
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
static int8_t ReadDataMemory(uint16_t aAddr, uint8_t *aDest, uint16_t aLen) {
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
  if (WriteRegister(REG_MAC, addr, 2) < 0) {
    SendStop();
    return -1;
  }
  if (ReadRegister(REG_MAC_DATA, buf, sizeof(buf)) < 0) {
    SendStop();
    return -1;
  }

  cal_checksum = addr[0] + addr[1];
  for (int i = 0; i < sizeof(buf); i++) {
    cal_checksum += buf[i];
  }
  cal_checksum = 0xff - cal_checksum;

  // Read and check checksum
  uint8_t reg_checksum[2];
  if (ReadRegister(REG_MAC_DATASUM, reg_checksum, sizeof(reg_checksum)) < 0) {
    SendStop();
    return -1;
  }
  if (reg_checksum[0] != cal_checksum) {
    printf("ERROR. Battery ReadDataMemory() at 0x%04X Checksum error.\n", aAddr);
    return -1;
  }

  //
  memcpy(aDest, buf, aLen);
  return 0;
}

//==========================================================================
// Change Data Memory
//==========================================================================
static int8_t ChangeDataMemory(uint16_t aAddr, uint8_t *aSrc, uint16_t aLen) {
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
  if (WriteRegister(REG_MAC, addr_buf, sizeof(addr_buf)) < 0) {
    SendStop();
    return -1;
  }
  if (ReadRegister(REG_MAC_DATA, data_buf, sizeof(data_buf)) < 0) {
    SendStop();
    return -1;
  }
  // DEBUG_HEX2STRING("REG_MAC_DATA: ", data_buf, sizeof(data_buf));

  cal_checksum = addr_buf[0] + addr_buf[1];
  for (int i = 0; i < sizeof(data_buf); i++) {
    cal_checksum += data_buf[i];
  }
  cal_checksum = 0xff - cal_checksum;

  // Read and check checksum
  if (ReadRegister(REG_MAC_DATASUM, checksum_buf, sizeof(checksum_buf)) < 0) {
    SendStop();
    return -1;
  }
  // DEBUG_HEX2STRING("REG_MAC_DATASUM: ", checksum_buf, sizeof(checksum_buf));
  if (checksum_buf[0] != cal_checksum) {
    printf("ERROR. Battery ChangeDataMemory() at 0x%04X Checksum error.\n", aAddr);
    return -1;
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
  if (WriteRegister(REG_CONTROL, subcommand, sizeof(subcommand)) < 0) {
    SendStop();
    return -1;
  }

  bool success = false;
  uint16_t op_status;

  for (int i = 15; i > 0; i--) {
    vTaskDelay(50 / portTICK_PERIOD_MS);
    if (ReadRegister(0x3a, buf, 2) < 0) {
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
    return -1;
  }

  // Write block
  // DEBUG_HEX2STRING("REG_MAC: ", addr_buf, sizeof(addr_buf));
  // DEBUG_HEX2STRING("REG_MAC_DATA: ", data_buf, sizeof(data_buf));
  // DEBUG_HEX2STRING("REG_MAC_DATASUM: ", checksum_buf, sizeof(checksum_buf));

  if (WriteRegister(REG_MAC, addr_buf, sizeof(addr_buf)) < 0) {
    SendStop();
    return -1;
  }
  if (WriteRegister(REG_MAC_DATA, data_buf, sizeof(data_buf)) < 0) {
    SendStop();
    return -1;
  }
  if (WriteRegister(REG_MAC_DATASUM, checksum_buf, sizeof(checksum_buf)) < 0) {
    SendStop();
    return -1;
  }

  // EXIT_CFG_UPDATE_REINIT
  PackU16Le(subcommand, 0x0091);
  if (WriteRegister(REG_CONTROL, subcommand, sizeof(subcommand)) < 0) {
    SendStop();
    return -1;
  }
  vTaskDelay(10 / portTICK_PERIOD_MS);
  if (ReadRegister(0x3a, buf, 2) < 0) {
    SendStop();
    return -1;
  }
  op_status = UnpackU16Le(buf);
  if ((op_status & 0x0400) != 0) {
    printf("ERROR. Battery ChangeDataMemory() failed to exit config update.\n");
    return -1;
  }
  DEBUG_PRINTLINE("Battery exit config update");

  return 0;
}

//==========================================================================
// Init
//==========================================================================
void BatteryInit(void) {}

//==========================================================================
// Get Result
//==========================================================================
int8_t BatteryGetResult(BatteryResult_t *aResult) {
  uint8_t buf[8];

  // init values
  aResult->valid = false;
  aResult->current = NAN;
  aResult->voltage = NAN;
  aResult->percentage = NAN;
  aResult->designCapacity = 0;
  aResult->fullChargeCapacity = 0;
  aResult->remainingCapacity = 0;

  //
  if (ReadRegister(0x3a, buf, 2) < 0) {
    printf("ERROR. BQ27220 read op status failed.\n");
    return -1;
  }
  // DEBUG_PRINTLINE("Battery OpStatus()=0x%04X", UnpackU16Le(buf));

  if (ReadRegister(0x0a, buf, 2) < 0) {
    printf("ERROR. BQ27220 read battery status failed.\n");
    return -1;
  }
  // DEBUG_PRINTLINE("Battery BatteryStatus()=0x%04X", UnpackU16Le(buf));

  if (ReadRegister(0x08, buf, 2) < 0) {
    printf("ERROR. BQ27220 read voltage failed.\n");
    return -1;
  }
  uint16_t raw_voltage = UnpackU16Le(buf);
  aResult->voltage = (float)raw_voltage / 1000;
  // DEBUG_PRINTLINE("Battery Voltage()=%.2fV (0x%04X)", gBatteryInfo.voltage, raw_voltage);

  if (ReadRegister(0x0C, buf, 2) < 0) {
    printf("ERROR. BQ27220 read current failed.\n");
    return -1;
  }
  uint16_t raw_current = UnpackU16Le(buf);
  if ((raw_current & 0x8000) != 0) {
    uint16_t current = (raw_current ^ 0xffff) + 1;
    aResult->current = -1 * (float)current / 1000;
  } else {
    aResult->current = (float)raw_current / 1000;
  }
  // DEBUG_PRINTLINE("Battery Current()=%.2fA (0x%04X)", gBatteryInfo.current, raw_current);

  // Capacity info
  if (ReadRegister(0x3c, buf, 2) < 0) {
    printf("ERROR. BQ27220 read design capacity failed.\n");
    return -1;
  }
  aResult->designCapacity = UnpackU16Le(buf);
  // DEBUG_PRINTLINE("Battery designCapacity=%dmAh (0x%04X)", gBatteryInfo.designCapacity, gBatteryInfo.designCapacity);

  if (ReadRegister(0x12, buf, 2) < 0) {
    printf("ERROR. BQ27220 read full charge capacity failed.\n");
    return -1;
  }
  aResult->fullChargeCapacity = UnpackU16Le(buf);
  // DEBUG_PRINTLINE("Battery fullChargeCapacity=%dmAh (0x%04X)", gBatteryInfo.fullChargeCapacity,
  // gBatteryInfo.fullChargeCapacity);

  if (ReadRegister(0x10, buf, 2) < 0) {
    printf("ERROR. BQ27220 read remaining capacity failed.\n");
    return -1;
  }
  aResult->remainingCapacity = UnpackU16Le(buf);
  // DEBUG_PRINTLINE("Battery remainingCapacity=%dmAh (0x%04X)", gBatteryInfo.remainingCapacity, gBatteryInfo.remainingCapacity);
  if (ReadRegister(0x2c, buf, 2) < 0) {
    printf("ERROR. BQ27220 read battery percentage failed.\n");
    return -1;
  }
  uint16_t state_of_charge = UnpackU16Le(buf);
  //    DEBUG_PRINTLINE("Battery StateOfCharge=%d (0x%04X)", state_of_charge, state_of_charge);
  if (state_of_charge > 100) state_of_charge = 100;
  aResult->percentage = (float)state_of_charge;

  //
  // ReadDataMemory(0x929f, buf, 2);
  // DEBUG_PRINTLINE("0x929f (Design Capacity): %02X %02X (%dmAh)", buf[0], buf[1], UnpackU16(buf));

  // if (ReadDataMemory(0x92A3, buf, 2) < 0) {
  //   printf("ERROR. BQ27220 read design voltage failed.\n");
  //   return -1;
  // }
  // DEBUG_PRINTLINE("0x92A3 (Design Voltage): %02X %02X (%.3fV)", buf[0], buf[1], aResult->designVoltage);

  // ReadDataMemory(0x9217, buf, 2);
  // DEBUG_PRINTLINE("0x9217 (Sleep Current): %02X %02X (%dmA)", buf[0], buf[1], UnpackU16(buf));

  // ReadDataMemory(0x9206, buf, 2);
  // DEBUG_HEX2STRING("0x9206 (Operation Config A): ", buf, 2);

  // ReadDataMemory(0x9208, buf, 2);
  // DEBUG_HEX2STRING("0x9208 (Operation Config B): ", buf, 2);

  // All done
  aResult->valid = true;
  return 0;
}

//==========================================================================
// Set Design Capacity
//==========================================================================
int8_t BatterySetDesignValue(uint16_t aCapacity, uint16_t aVoltage_mV) {
  int8_t ret = 0;
  uint8_t buf[8];

  if (aCapacity > 32767) aCapacity = 32767;
  if (aVoltage_mV > 32767) aVoltage_mV = 32767;

  PackU16(&buf[0], aCapacity);
  PackU16(&buf[2], aCapacity);
  if (ChangeDataMemory(0x929d, buf, 4) < 0) ret = -1;

  PackU16(buf, aVoltage_mV);
  if (ChangeDataMemory(0x92A3, buf, 2) < 0) ret = -1;

  ReadDataMemory(0x929f, buf, 2);
  DEBUG_PRINTLINE("0x929f (Design Capacity): %02X %02X (%dmAh)", buf[0], buf[1], UnpackU16(buf));

  ReadDataMemory(0x92A3, buf, 2);
  DEBUG_PRINTLINE("0x92A3 (Design Voltage): %02X %02X (%dmV)", buf[0], buf[1], UnpackU16(buf));

  return ret;
}

//==========================================================================
// Get Design Capacity
//==========================================================================
int8_t BatteryGetDesignValue(uint16_t *aCapacity, uint16_t *aVoltage_mV) {
  uint8_t buf[8];

  if (ReadRegister(0x3c, buf, 2) < 0) {
    printf("ERROR. BQ27220 read design capacity failed.\n");
    return -1;
  }
  *aCapacity = UnpackU16Le(buf);

  if (ReadDataMemory(0x92A3, buf, 2) < 0) {
    printf("ERROR. BQ27220 read design voltage failed.\n");
    return -1;
  }
  *aVoltage_mV = UnpackU16(buf);

  return 0;
}