//==========================================================================
// Test
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
#include "test.h"

#include "accelerometer.h"
#include "air_quality.h"
#include "app_utils.h"
#include "barometric_pressure.h"
#include "battery.h"
#include "debug.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "led.h"
#include "mx_target.h"
#include "sleep.h"

//==========================================================================
// Defines
//==========================================================================
#if defined(CONFIG_MATCHX_BATTERY_DESING_CAPACITY)
#define MATCHX_BATTERY_DESING_CAPACITY CONFIG_MATCHX_BATTERY_DESING_CAPACITY
#else
#define MATCHX_BATTERY_DESING_CAPACITY 1200
#endif

#define BATTERY_DESING_VOLTAGE_mV 3700

//==========================================================================
// Constants
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
// Flush keys
//==========================================================================
static void FlushInput(void) {
  for (;;) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    int key_in = getc(stdin);
    if (key_in < 0) break;
  }
}

//==========================================================================
// Send Stop
//==========================================================================
static void SendI2CStop(void) {
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  i2c_master_stop(i2c_link);
  i2c_master_cmd_begin(I2CNUM, i2c_link, 10 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(i2c_link);
}

//==========================================================================
// Scan I2C
//==========================================================================
static bool TestI2cAddr(uint8_t aAddr) {
  bool ret = false;
  uint8_t buf[1];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  for (;;) {
    // Construct the transaction
    if (i2c_master_start(cmd) != ESP_OK) {
      PrintLine("ERROR. i2c_master_start() failed.");
      break;
    }
    if (i2c_master_write_byte(cmd, (aAddr << 1) | I2C_MASTER_READ, true) != ESP_OK) {
      PrintLine("ERROR. i2c_master_write_byte() failed.");
      break;
    }
    if (i2c_master_read(cmd, buf, sizeof(buf), I2C_MASTER_LAST_NACK) != ESP_OK) {
      PrintLine("ERROR. i2c_master_write_byte() failed.");
      break;
    }
    if (i2c_master_stop(cmd) != ESP_OK) {
      PrintLine("ERROR. i2c_master_stop() failed.");
      break;
    }

    // Start the transaction
    if (i2c_master_cmd_begin(I2CNUM, cmd, 200 / portTICK_PERIOD_MS)) {
      //      PrintLine("ERROR. i2c_master_cmd_begin() failed.");
      break;
    }

    // all done
    ret = true;
    break;
  }
  i2c_cmd_link_delete(cmd);

  //
  return ret;
}

static void ScanI2C(void) {
  PrintLine("Scanning address 0x00 to 0x7f");
  for (uint8_t i = 0; i < 0x80; i++) {
    if (TestI2cAddr(i)) {
      const char *dev = "UNKNOWN";
      switch (i) {
        case 0x18:
          dev = "Accelerometer LIS2DE12";
          break;
        case 0x32:
          dev = "Air Quality Sensor ZMOD4410";
          break;
        case 0x76:
          dev = "Pressure Sensor BME280";
          break;
        case 0x55:
          dev = "Battery Fuel Gauge BQ27220YZFR";
          break;
        case 0x49:
          dev = "Security chip A71CH";
          break;
      }
      PrintLine("Found device at 0x%02X, %s", i, dev);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Finished.");
}

//==========================================================================
// Test Air quality
//==========================================================================
static void TestAirQuality(void) {
  uint32_t tick_measure = 0;

  PrintLine("TestAirQuality. Press [ENTER] to exit.");
  FlushInput();
  for (;;) {
    // Exit if ENTER pressed
    int key_in = getc(stdin);
    if (key_in >= 0) {
      DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
    }
    if ((key_in == 0x0a) || (key_in == 0x0d)) {
      break;
    }

    //
    if ((tick_measure == 0) || (TickElapsed(tick_measure) >= AirQualityMeasInterval())) {
      tick_measure = GetTick();
      LedSet(true);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      LedSet(false);

      // Kick start measurement
      AirQualityStartMeas();

      // Wait measurement
      vTaskDelay(AirQualityMeasTime() / portTICK_PERIOD_MS);

      // Get Result
      AirQualityResult_t result;
      if (AirQualityGetResult(&result, NAN, NAN) >= 0) {
        if (result.valid) {
          PrintLine("Result: etoh=%.3f ppm, tvoc=%.3f mg/m^3, eco2=%.0f ppm, iaq=%.1f", result.etoh, result.tvoc, result.eco2,
                    result.iaq);
        } else if (result.warmUp) {
          PrintLine("Warm up in progress.");
        }
      }
    }

    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Stopped.");
}

//==========================================================================
// Test Pressure Sensor
//==========================================================================
static void TestBarometricPressure(void) {
  uint32_t tick_measure = 0;

  PrintLine("TestBarometricPressure. Press [ENTER] to exit.");
  FlushInput();
  for (;;) {
    // Exit if ENTER pressed
    int key_in = getc(stdin);
    if (key_in >= 0) {
      DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
    }
    if ((key_in == 0x0a) || (key_in == 0x0d)) {
      break;
    }

    //
    if (TickElapsed(tick_measure) > BarometricPressureMeasTime()) {
      if (BarometricPressureIsMeasDone()) {
        BarometricPressureResult_t result;
        if (BarometricPressureGetResult(&result) >= 0) {
          PrintLine("Result: P=%.3fhPa, T=%.3fC, H=%.3f%%RH", result.pressure, result.temperature, result.humidity);
        }

        // Kick start measurement again
        BarometricPressureStartMeas();
        tick_measure = GetTick();

        //
        LedSet(true);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        LedSet(false);
      } else {
        PrintLine("Result not ready.");
      }
    }

    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Stopped.");
}

//==========================================================================
// Set battery design values
//==========================================================================
static void SetBatteryDesignValues(void) {
  uint16_t design_capacity;
  uint16_t design_voltage_mv;

  // Set design capacity and voltage
  if (BatteryGetDesignValue(&design_capacity, &design_voltage_mv) < 0) {
    PrintLine("BatteryGetDesignValue failed. Please try power off/on again.");
    // Uncondition set
    BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV);
    return;
  }

  if ((design_capacity != MATCHX_BATTERY_DESING_CAPACITY) || (design_voltage_mv != BATTERY_DESING_VOLTAGE_mV)) {
    if (BatterySetDesignValue(MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV) < 0) {
      PrintLine("Change design capacity failed.");
      return;
    }
    PrintLine("INFO. Change design capacity to %dmAh %dmV success.", MATCHX_BATTERY_DESING_CAPACITY, BATTERY_DESING_VOLTAGE_mV);
  } else {
    PrintLine("Battery Design Value: %dmAh %dmV", design_capacity, design_voltage_mv);
  }
}


//==========================================================================
// Test Battery
//==========================================================================
static void TestBattery(void) {
  uint32_t tick_measure = 0;
  BatteryResult_t result;

  //
  PrintLine("TestBattery");
  SetBatteryDesignValues();
  PrintLine("Press [ENTER] to exit.");
  FlushInput();
  for (;;) {
    // Exit if ENTER pressed
    int key_in = getc(stdin);
    if (key_in >= 0) {
      DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
    }
    if ((key_in == 0x0a) || (key_in == 0x0d)) {
      break;
    }

    //
    if ((tick_measure == 0) || (TickElapsed(tick_measure) > 10000)) {
      tick_measure = GetTick();
      if (BatteryGetResult(&result) < 0) {
        PrintLine("BatteryGetResult failed.");
      } else {
        PrintLine("Battery: %.3fV, %.3fA, %.1f%%", result.voltage, result.current, result.percentage);
      }
    }

    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Stopped.");
}

//==========================================================================
// Test Accelerometer
//==========================================================================
static void TestAccelerometer(void) {
  AccelerometerResult_t result;

  //
  PrintLine("TestAccelerometer. Press [ENTER] to exit.");
  FlushInput();
  for (;;) {
    // Exit if ENTER pressed
    int key_in = getc(stdin);
    if (key_in >= 0) {
      DEBUG_PRINTLINE("key pressed %d (0x%X)", key_in, key_in);
    }
    if ((key_in == 0x0a) || (key_in == 0x0d)) {
      break;
    }

    //
    if (AccelerometerIsDataReady()) {
      if (AccelerometerGetResult(&result) < 0) {
        PrintLine("AccelerometerGetResult failed.");
      } else {
        PrintLine("Accelerometer: x=%.3f, y=%.3f, z=%.3f", result.acce_x, result.acce_y, result.acce_z);
      }
    }

    //
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  SendI2CStop();
  PrintLine("Stopped.");
}

//==========================================================================
// Top level
//==========================================================================
void TestGo(void) {
  //
  if (IsWakeByReset()) {
    PrintLine("Boot from reset.");
  }

  //
  MxTargetInit();
  LedInit();
  AirQualityInit();
  BarometricPressureInit();
  BatteryInit();
  AccelerometerInit();

  //
  for (;;) {
    // Show menu
    PrintLine("=================================");
    PrintLine("test_i2c_devices");
    PrintLine("=================================");
    PrintLine("  1) Scan devices");
    PrintLine("  2) Test Air Quality");
    PrintLine("  3) Test Barometric Pressure");
    PrintLine("  4) Test Battery");
    PrintLine("  5) Test Accelerometer");

    // Wait key
    int key_in = 0;
    while (key_in == 0) {
      // Read input from console
      key_in = getc(stdin);
      if (key_in >= 0) {
        break;
      } else {
        key_in = 0;
      }
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Tests
    switch (key_in) {
      case '1':
        ScanI2C();
        break;
      case '2':
        TestAirQuality();
        break;
      case '3':
        TestBarometricPressure();
        break;
      case '4':
        TestBattery();
        break;
      case '5':
        TestAccelerometer();
        break;
    }

    // Flush
    FlushInput();
  }
}