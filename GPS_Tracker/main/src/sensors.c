//==========================================================================
// Application
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
#include "sensors.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "GPS_Distance.h"
#include "esp_sleep.h"

#include "app_utils.h"
#include "lora_compon.h"
#include "packer.h"
#include "matchx_payload.h"
#include "bq27220.h"
#include "lis2de12.h"
#include "ubxlib.h"
#include "led.h"
#include "sleep.h"

//==========================================================================
// Defines
//==========================================================================
#define TAG "sensors.c"

/********************************Idle detection ********************************/
#define IDLE_ACCEL_THRESHOLD 300     // Acceleration In mG
#define IDLE_LATCH_TIME 30000        // In milliseconds

/********************************GPS Data Filter ********************************/
#define MAX_RADIUS  10000         // Maximum radius of location (Accuracy) in millimeters
#define MIN_SVS 5                 // Minimum number of space vehicles

/********************************GPS Data Queue ********************************/
#define GPS_QUEUE_SIZE 500        // Queue size to buffer coordinates, in case of the connection is lost and you need to keep all data
#define GPS_QUEUE_INTERVAL 150     // The time interval to push a new GPS Coordinates into the queue in seconds

/********************************GPS Module ********************************/
#define U_CFG_GNSS_MODULE_TYPE U_GNSS_MODULE_TYPE_M7
#define U_CFG_APP_GNSS_UART 1
#define U_CFG_APP_PIN_GNSS_TXD  17
#define U_CFG_APP_PIN_GNSS_RXD  18
#define U_CFG_APP_PIN_GNSS_ENABLE_POWER 38
#define U_CFG_APP_PIN_GNSS_CTS  -1
#define U_CFG_APP_PIN_GNSS_RTS  -1

/********************************Accelerometer ********************************/
#define ACCEL_INT GPIO_NUM_3

//==========================================================================
// Variables
//==========================================================================
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg = {
        .cfgGnss = {
            .moduleType = U_CFG_GNSS_MODULE_TYPE,
            .pinEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
            .pinDataReady = -1  // Not used
            }, },

    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_GNSS_UART,
            .baudRate = U_GNSS_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
            .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
            .pinCts = U_CFG_APP_PIN_GNSS_CTS,
            .pinRts = U_CFG_APP_PIN_GNSS_RTS }, }, };

// NETWORK configuration for GNSS
static const uNetworkCfgGnss_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_GNSS,
    .moduleType = U_CFG_GNSS_MODULE_TYPE,
    .devicePinPwr = -1,
    .devicePinDataReady = -1 };

typedef struct {
  uLocation_t GPS_Data;
  double TotalDistance;         // In meters
  double TotalAverageDistance;  // In meters
} GPSData_t;

GPSData_t gGPSData;

uDeviceHandle_t gDevHandle = NULL;

uint8_t gGPSDataCount = 0;

GPSData_t gGPS;
QueueHandle_t gGPS_queue;

BQ27220_TypeDef gBat_Monitor;

lis2de12_config_t glis2de12_config = LIS2DE12_CONFIG_DEFAULT()
;

bool gIsAwake = false;
bool gIsFix = false;
bool gIsMoving = false;
bool gIsVehicle = false;
bool gReadyToSleep = false;

//==========================================================================
// Get Battery status from the Battery monitor
//==========================================================================
esp_err_t BatteryStatusUpdate() {
  default_init(&gBat_Monitor);
  esp_err_t err = read_registers(&gBat_Monitor);

  ESP_LOGI(TAG, "Battery Status Update: SOC = %d ,Current = %d, Voltage = %d", gBat_Monitor.socReg, gBat_Monitor.currentReg,
           gBat_Monitor.voltReg);

  if (err != ESP_OK) {
    LoRaComponSetBatteryPercent(NAN);
    ESP_LOGE(TAG, "Battery Status unavailable");
    return err;
  } else if (gBat_Monitor.currentReg >= 0) {
    LoRaComponSetExtPower();
    return err;
  } else {
    LoRaComponSetBatteryPercent((float) gBat_Monitor.socReg);
    return err;
  }
}

//==========================================================================
// Transmit data
//==========================================================================
uint32_t DataWaiting() {
  return (uint32_t) uxQueueMessagesWaiting(gGPS_queue);
}

//==========================================================================
// Transmit data
//==========================================================================
uint16_t GPS_SendData() {
  uint8_t buf[64];
  uint8_t *ptr;

  ESP_LOGI(TAG, "GPS_SendData.\n");
  if (gGPS_queue) {
    if (xQueueReceive(gGPS_queue, &gGPS, (TickType_t) 5)) {
      ESP_LOGI(TAG, "Packing Data.\n");
      ptr = buf;

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_DISTANCE);
      ptr += PackFloat(ptr, (float32_t) (gGPS.TotalAverageDistance * 1000));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_DISTANCE);
      ptr += PackFloat(ptr, (float32_t) (gGPS.TotalDistance * 1000));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x0C);
      ptr += PackU8(ptr, MX_SENSOR_GPS);
      ptr += PackU8(ptr, 1);
      ptr += PackFloat(ptr, (gGPS.GPS_Data.latitudeX1e7 / 10000000.0));
      ptr += PackFloat(ptr, (gGPS.GPS_Data.longitudeX1e7 / 10000000.0));
      ptr += PackU16(ptr, (uint16_t) ((gGPS.GPS_Data.altitudeMillimetres / 1000.0) * 10));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_SPEED);
      ptr += PackFloat(ptr, (float32_t) (gGPS.GPS_Data.speedMillimetresPerSecond / 1000.0));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_CURRENT);
      ptr += PackFloat(ptr, (float32_t) (gBat_Monitor.currentReg / 1000.0));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_VOLTAGE);
      ptr += PackFloat(ptr, (float32_t) (gBat_Monitor.voltReg / 1000.0));

      ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
      ptr += PackU8(ptr, MX_SENSOR_COUNTER32);
      ptr += PackU32(ptr, (uint32_t) (uxQueueMessagesWaiting(gGPS_queue)));

      uint16_t tx_len = ptr - buf;
      Hex2String("Sending ", buf, tx_len);
      LoRaComponSendData(buf, tx_len);
      ESP_LOGI(TAG, "Send data to buffer.\n");

      printf("Lat = %0.3f, Lon = %0.3f, Speed = %0.3f\n", (gGPS.GPS_Data.latitudeX1e7 / 10000000.0),
             (gGPS.GPS_Data.longitudeX1e7 / 10000000.0), (gGPS.GPS_Data.speedMillimetresPerSecond / 1000.0));
      printf("SVS %d\n", gGPS.GPS_Data.svs);

      return tx_len;
    }
  }
  return 0;
}

//==========================================================================
// Initialize Accelerometer
//==========================================================================
esp_err_t AccelerometerInit() {

  glis2de12_config.fs = LIS2DE12_2g;
  glis2de12_config.odr = LIS2DE12_ODR_10Hz;
  glis2de12_config.fds = LIS2DE12_ENABLE;
  glis2de12_config.hpcf_cutoff = LIS2DE12_LIGHT;
  glis2de12_config.hpm_mode = LIS2DE12_NORMAL_WITH_RST;

  return lis2de12_initialization(I2C_NUM_0, &glis2de12_config);
}

//==========================================================================
// Initialize GPS
//==========================================================================
esp_err_t GPS_Init() {

  int32_t returnCode;

  gGPS_queue = xQueueCreate(GPS_QUEUE_SIZE, sizeof(gGPSData));
  if (gGPS_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create GPS queue");
    return ESP_FAIL;
  }

  // Initialise the APIs we will need
  uPortInit();
  uDeviceInit();
  // Open the device
  returnCode = uDeviceOpen(&gDeviceCfg, &gDevHandle);
  ESP_LOGI(TAG, "Opened GPS device with return code %d.\n", returnCode);

  if (returnCode == 0) {
    // You may configure GNSS as required here
    // here using any of the GNSS API calls.

    // Bring up the GNSS network interface
    ESP_LOGI(TAG, "Bringing up the GNSS network...\n");
    if (uNetworkInterfaceUp(gDevHandle, U_NETWORK_TYPE_GNSS, &gNetworkCfg) == 0) {
      ESP_LOGI(TAG, "GPS successfully configured.\n");
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "Unable to bring up GNSS!\n");
      // Close the device
      uDeviceClose(gDevHandle, true);
      // Tidy up
      uDeviceDeinit();
      uPortDeinit();
      return ESP_FAIL;
    }
  } else {
    ESP_LOGE(TAG, "Unable to bring up the device!\n");
    // Tidy up
    uDeviceDeinit();
    uPortDeinit();
    return ESP_FAIL;
  }

  return ESP_FAIL;
}

//==========================================================================
// Sensors Task
//==========================================================================
esp_err_t SensorsInit() {
  esp_err_t returnCode;

  returnCode = AccelerometerInit();

  if (returnCode != ESP_OK) {
    return returnCode;
  }

  returnCode = GPS_Init();
  return returnCode;
}

//==========================================================================
// Callback function to receive location.
//==========================================================================
static void GPS_Callback(uDeviceHandle_t gDevHandle, int32_t errorCode, const uLocation_t *pLocation) {
  if (errorCode == 0) {
    gGPSData.GPS_Data = *pLocation;

    double latitude = gGPSData.GPS_Data.latitudeX1e7 / 10000000.0;  //convert to degrees
    double longitude = gGPSData.GPS_Data.longitudeX1e7 / 10000000.0;  //convert to degrees
    double speed = gGPSData.GPS_Data.speedMillimetresPerSecond / 1000.0;  //convert to m/s

    if ((gGPSData.GPS_Data.svs >= MIN_SVS) && (gGPSData.GPS_Data.radiusMillimetres <= MAX_RADIUS)) {  // Readings quality check

      gIsFix = true;
      LedSet(LED_MODE_FAST_BLINKING, 2, 1);
      if (gGPSDataCount >= GPS_QUEUE_INTERVAL) {
        ESP_LOGI(TAG, "Add coordinate to queue.\n");
        xQueueSend(gGPS_queue, (void* ) &gGPSData, (TickType_t ) 0);
        gGPSDataCount = 0;
      } else {
        gGPSDataCount++;
      }

      if (((speed > 0.7) && gIsMoving) || (speed > 4.4)) {      // Check if not Idle
        gGPSData.TotalDistance = UpdateRawDistance(latitude, longitude);
        gGPSData.TotalAverageDistance = UpdateAverageDistance(latitude, longitude);

        if (speed > 4.4) {  // Check if it's a vehicle movement to neglect the Accelerometer movement detection
          gIsVehicle = true;
        } else {
          gIsVehicle = false;
        }

      } else {

      }

    } else {
//      LedSet(LED_MODE_ON, -1);
    }
  }
}

//==========================================================================
// Sleep
//==========================================================================
void SensorPrepareForSleep(void) {
  if (gSensorTaskHandle != NULL) {
    // Set gReadyToSleep flag
    LedSet(LED_MODE_FAST_BLINKING, 5, 0);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    gReadyToSleep = true;
    vTaskSuspend(gSensorTaskHandle);
  }
}

void SensorResumeFromSleep(void) {
  if (gSensorTaskHandle != NULL) {
    // Reset gReadyToSleep flag
    LedSet(LED_MODE_ON, -1, 0);
    gReadyToSleep = false;
    vTaskResume(gSensorTaskHandle);
  }
}

bool SensorSleepReady(void) {
  return gReadyToSleep;
}

//==========================================================================
// Set GPS to sleep
//==========================================================================
static void GPS_Sleep() {
  ESP_LOGI(TAG, "Prepare GPS to sleep.\n");
  // Stop getting location
  uLocationGetStop(gDevHandle);
  ESP_LOGI(TAG, "Cancel uLocationGet.\n");
  if (uGnssPwrOffBackup(gDevHandle) == 0) {
    ESP_LOGI(TAG, "GPS Backup mode.\n");
    gIsAwake = false;
  }
}

//==========================================================================
// Set GPS to wake-up
//==========================================================================
static void GPS_Wakeup() {
  ESP_LOGI(TAG, "Prepare GPS to wake up.\n");
  uLocationGetStop(gDevHandle);
  gIsAwake = true;
  int32_t returnCode;
  if (uGnssPwrOn(gDevHandle) == 0)
    ESP_LOGI(TAG, "GPS Woke up.\n");

  ESP_LOGI(TAG, "Starting continuous location.\n");
  returnCode = uLocationGetContinuousStart(gDevHandle, U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS, U_DEVICE_TYPE_GNSS, NULL, NULL,
                                           GPS_Callback);

  ESP_LOGI(TAG, "Waiting up for location fixes. %d\n", returnCode);
}

//==========================================================================
// Get Idle signal from the Accelerometer
//==========================================================================
static void IdleTimerCallback() {
  gIsMoving = false;
}

//==========================================================================
// Sensors Task
//==========================================================================
void SensorsTask(void *args) {
  int32_t returnCode;
  lis2de12_acce_value_t Accel;
  double Mag;

  /********************************Set Accelerometer INT1 ********************************/

  // High-pass filter enabled on interrupt activity 1
  lis2de12_high_pass_int_conf_set(&dev_ctx, LIS2DE12_ON_INT1_GEN);

  // Interrupt activity 1 driven to INT1 pad
  lis2de12_ctrl_reg3_t ctrl_reg3 = {
      .i1_ia1 = 1 };
  lis2de12_pin_int1_config_set(&dev_ctx, &ctrl_reg3);

  // Set INT1 Polarity
  lis2de12_ctrl_reg6_t ctrl_reg6 = {
      .int_polarity = 1 };
  lis2de12_pin_int2_config_set(&dev_ctx, &ctrl_reg6);

  // Set INT1 Threshold
  lis2de12_int1_gen_threshold_set(&dev_ctx, IDLE_ACCEL_THRESHOLD / 16.0);

  // Set INT1 Duration
  lis2de12_int1_gen_duration_set(&dev_ctx, 10);

  // Dummy read to force the HP filter to current acceleration value
  uint8_t acc_ref;
  lis2de12_filter_reference_get(&dev_ctx, &acc_ref);

  // Configure desired wake-up event
  lis2de12_int1_cfg_t int1_cfg = {
      .xlie = 1,
      .ylie = 1,
      .zlie = 1,
      ._6d = 0,
      .aoi = 1 };
  lis2de12_int1_gen_conf_set(&dev_ctx, &int1_cfg);

  /********************************Set GPS Continuous Reading ********************************/
  // Switch off NMEA messages
  uGnssCfgSetProtocolOut(gDevHandle, U_GNSS_PROTOCOL_NMEA, false);

  ESP_LOGI(TAG, "Starting continuous location.\n");
  returnCode = uLocationGetContinuousStart(gDevHandle, U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS, U_DEVICE_TYPE_GNSS, NULL, NULL,
                                           GPS_Callback);

  ESP_LOGI(TAG, "Waiting up for location fixes. %d\n", returnCode);
  gIsAwake = true;

  LedSet(LED_MODE_SLOW_BLINKING, -1, 0);
  while (!gIsFix) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  ESP_LOGI(TAG, "GPS Location Fixes Done.\n");

//  vTaskDelay(420000 / portTICK_PERIOD_MS);

  TimerHandle_t idleTimer = xTimerCreate("Move_Trigger", (IDLE_LATCH_TIME / portTICK_RATE_MS), pdFALSE, 0, IdleTimerCallback);
  xTimerStart(idleTimer, 0);
  gIsMoving = true;

  LedSet(LED_MODE_ON, -1, 0);

  for (;;) {

    if (lis2de12_Is_data_ready()) {
      lis2de12_get_acce(&Accel);
      // Dummy read to force the HP filter to current acceleration value
      lis2de12_filter_reference_get(&dev_ctx, &acc_ref);
      Mag = sqrt(pow(Accel.acce_x, 2) + pow(Accel.acce_y, 2) + pow(Accel.acce_z, 2));
      if ((Mag > IDLE_ACCEL_THRESHOLD) || gIsVehicle) {
        xTimerStart(idleTimer, 0);
        gIsMoving = true;
      }
    }

    if (gIsMoving) {
      if (!gIsAwake) {
        GPS_Wakeup();
      }
    } else {
      if (gIsAwake) {
        GPS_Sleep();
      }
      ESP_LOGI(TAG, "Sleep request.\n");

      SensorPrepareForSleep();

      ESP_LOGI(TAG, "Wake up.\n");
      xTimerStart(idleTimer, 0);
      gIsMoving = true;
    }

//    if (gpio_get_level(ACCEL_INT)) {
//      LedSet(true);
//    } else {
//      LedSet(false);
//    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  ESP_LOGI(TAG, "SensorsTask ended.");
  gSensorTaskHandle = NULL;
  vTaskDelete(NULL);
}
