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
#include "bq27220.h"
#include "lora_compon.h"
#include "lis2de12.h"
#include "led.h"
#include "matchx_payload.h"
#include "GPS_Distance.h"
#include "packer.h"
#include "app_utils.h"

#define TAG "sensors.c"

/********************************Idle detection ********************************/
#define Idle_Accel_THRESHOLD 300    // Acceleration In mG
#define Idle_Signal_Latch_Time 500  // In milliseconds

//==========================================================================
// Get Battery status from the Battery monitor
//==========================================================================
BQ27220_TypeDef Bat_Monitor;
esp_err_t BatteryStatusUpdate() {
  default_init(&Bat_Monitor);
  esp_err_t err = read_registers(&Bat_Monitor);

  ESP_LOGI(TAG, "Battery Status Update: SOC = %d ,Current = %d", Bat_Monitor.socReg, Bat_Monitor.currentReg);

  if (err != ESP_OK) {
    LoRaComponSetBatteryPercent(NAN);
    ESP_LOGE(TAG, "Battery Status unavailable");
    return err;
  } else if (Bat_Monitor.currentReg >= 0) {
    LoRaComponSetExtPower();
    return err;
  } else {
    LoRaComponSetBatteryPercent((float) Bat_Monitor.socReg);
    return err;
  }
}

//==========================================================================
// Transmit GPS data
//==========================================================================
GPSData_t gps;
QueueHandle_t GPS_queue;
bool Is_Moving = false;

void GPS_MeasureStart() {
  GPS_queue = GPS_Distane_INIT(&Is_Moving);
  if (GPS_queue == NULL)
    ESP_LOGE(TAG, "Failed to Init the GPS");

  xTaskCreate(Idle_Detection, "Idle_Detection", 2048, NULL, 0, NULL);
}

uint16_t GPS_SendData() {
  uint8_t buf[64];
  uint8_t *ptr;

  if (xQueueReceive(GPS_queue, &gps, (TickType_t) 5)) {
//          gpio_set_level(USR_LED, 0);
    ptr = buf;
    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
    ptr += PackU8(ptr, MX_SENSOR_DISTANCE);
    ptr += PackFloat(ptr, (float32_t) (gps.Total_Distance * 1000));

    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x0C);
    ptr += PackU8(ptr, MX_SENSOR_GPS);
    ptr += PackU8(ptr, 1);
    ptr += PackFloat(ptr, gps.gps_data.latitude);
    ptr += PackFloat(ptr, gps.gps_data.longitude);
    ptr += PackU16(ptr, (uint16_t) (gps.gps_data.altitude * 10));

    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
    ptr += PackU8(ptr, MX_SENSOR_SPEED);
    ptr += PackFloat(ptr, (float32_t) (gps.gps_data.speed));

    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
    ptr += PackU8(ptr, MX_SENSOR_CURRENT);
    ptr += PackFloat(ptr, (float32_t) (Bat_Monitor.currentReg));

    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
    ptr += PackU8(ptr, MX_SENSOR_VOLTAGE);
    ptr += PackFloat(ptr, (float32_t) (Bat_Monitor.voltReg));

    ptr += PackU8(ptr, MX_DATATYPE_SENSOR | 0x05);
    ptr += PackU8(ptr, MX_SENSOR_COUNTER32);
    ptr += PackU32(ptr, (uint32_t) (uxQueueMessagesWaiting(GPS_queue)));

    uint16_t tx_len = ptr - buf;
    Hex2String("Sending ", buf, tx_len);
    LoRaComponSendData(buf, tx_len);

    printf("Lat = %0.3f, Lon = %0.3f, Speed = %0.3f\n", gps.gps_data.latitude, gps.gps_data.longitude, gps.gps_data.speed);
    printf("Fix %d\n", gps.gps_data.fix);

    return tx_len;
  }
  return 0;
}

//==========================================================================
// Get Idle signal from the Accelerometer
//==========================================================================
static void Idle_Timer_callback() {
//  LedSet(false);
  Is_Moving = false;
}

void Idle_Detection(void *args) {

  lis2de12_config_t lis2de12_config = LIS2DE12_CONFIG_DEFAULT()
  ;

  lis2de12_config.fs = LIS2DE12_2g;
  lis2de12_config.odr = LIS2DE12_ODR_10Hz;
  lis2de12_config.fds = LIS2DE12_ENABLE;
  lis2de12_config.hpcf_cutoff = LIS2DE12_LIGHT;

  lis2de12_initialization(I2C_NUM_0, &lis2de12_config);

  TimerHandle_t Timer_0 = xTimerCreate("Move_Trigger", (Idle_Signal_Latch_Time / portTICK_RATE_MS), pdFALSE, 0,
                                       Idle_Timer_callback);

  lis2de12_acce_value_t Accel;
  double Mag;
  for (;;) {
    if (lis2de12_Is_data_ready()) {
      lis2de12_get_acce(&Accel);
      Mag = sqrt(pow(Accel.acce_x, 2) + pow(Accel.acce_y, 2) + pow(Accel.acce_z, 2));
//      printf("Mag = %6.1f\n", Mag);
      if (Mag > Idle_Accel_THRESHOLD) {
        xTimerStart(Timer_0, 0);
        Is_Moving = true;
//        LedSet(true);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
