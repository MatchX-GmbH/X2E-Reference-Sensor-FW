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

#include "app_utils.h"
#include "debug.h"
#include "driver/gpio.h"
#include "led.h"
#include "mx_target.h"
#include "sleep.h"
#include "ubxlib.h"

//==========================================================================
// Defines
//==========================================================================
// GPS
#define U_CFG_GNSS_MODULE_TYPE U_GNSS_MODULE_TYPE_M7
#define U_CFG_APP_GNSS_UART 1
#define U_CFG_APP_PIN_GNSS_TXD 17
#define U_CFG_APP_PIN_GNSS_RXD 18
#define U_CFG_APP_PIN_GNSS_ENABLE_POWER 38
#define U_CFG_APP_PIN_GNSS_CTS -1
#define U_CFG_APP_PIN_GNSS_RTS -1

//==========================================================================
// Constants
//==========================================================================
// GPS device config
static const uDeviceCfg_t kGpsDeviceConfig = {
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg =
        {
            .cfgGnss =
                {
                    .moduleType = U_CFG_GNSS_MODULE_TYPE,
                    .pinEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
                    .pinDataReady = -1  // Not used
                },
        },

    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg =
        {
            .cfgUart = {.uart = U_CFG_APP_GNSS_UART,
                        .baudRate = U_GNSS_UART_BAUD_RATE,
                        .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
                        .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
                        .pinCts = U_CFG_APP_PIN_GNSS_CTS,
                        .pinRts = U_CFG_APP_PIN_GNSS_RTS},
        },
};

// NETWORK configuration for GNSS
static const uNetworkCfgGnss_t kGpsNetworkConfig = {
    .type = U_NETWORK_TYPE_GNSS, .moduleType = U_CFG_GNSS_MODULE_TYPE, .devicePinPwr = -1, .devicePinDataReady = -1};

//==========================================================================
// Variables
//==========================================================================
uDeviceHandle_t gGpsHandle = NULL;

//==========================================================================
// GPS
//==========================================================================
//==========================================================================
// Callback function to receive location.
//==========================================================================
static void GpsCallbackFunc(uDeviceHandle_t aDevHandle, int32_t aErrorCode, const uLocation_t *aLocation) {
  if (aErrorCode == 0) {
    const uLocation_t *gps_data = aLocation;

    double latitude = (double)gps_data->latitudeX1e7 / 10000000.0;        // convert to degrees
    double longitude = (double)gps_data->longitudeX1e7 / 10000000.0;      // convert to degrees
    int32_t radis_m = gps_data->radiusMillimetres / 1000;
    int32_t svs = gps_data->svs;
    uint64_t timestamp = gps_data->timeUtc;

    DEBUG_PRINTLINE("GPS: %.5f,%.5f (%d), time=%llu, svs=%d, ", latitude, longitude, radis_m, timestamp, svs);
  }
  else {
    DEBUG_PRINTLINE("GpsCallbackFunc error=%d", aErrorCode);
  }
}

static int8_t GpsInit(void) {
  int32_t gps_ret;

  // Initialise the APIs we will need
  uPortInit();
  uDeviceInit();
//  uPortLogOff();
  // Open the device
  gps_ret = uDeviceOpen(&kGpsDeviceConfig, &gGpsHandle);
  if (gps_ret < 0) {
    PrintLine("ERROR. UBOX open device failed. %d", gps_ret);
    uDeviceDeinit();
    uPortDeinit();
    return -1;
  }

  // Bring up the GNSS network interface
  gps_ret = uNetworkInterfaceUp(gGpsHandle, U_NETWORK_TYPE_GNSS, &kGpsNetworkConfig);
  if (gps_ret < 0) {
    PrintLine("ERROR. Bring GNSS failed. %d", gps_ret);
    uDeviceClose(gGpsHandle, true);
    uDeviceDeinit();
    uPortDeinit();
    return -1;
  }

  // Switch off NMEA messages
  uGnssCfgSetProtocolOut(gGpsHandle, U_GNSS_PROTOCOL_NMEA, false);

  //
  DEBUG_PRINTLINE("GPS successfully configured.");
  return 0;
}

static int8_t GpsStart(void) {
  int32_t gps_ret;

  DEBUG_PRINTLINE("Start GPS.");
  uLocationGetStop(gGpsHandle);
  gps_ret = uGnssPwrOn(gGpsHandle);
  if (gps_ret < 0) {
    PrintLine("uGnssPwrOn failed. %d", gps_ret);
  }

  DEBUG_PRINTLINE("GPS start continuous location.");
  gps_ret = uLocationGetContinuousStart(gGpsHandle, U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS, U_DEVICE_TYPE_GNSS, NULL, NULL,
                                        GpsCallbackFunc);
  if (gps_ret < 0) {
    PrintLine("Start GPS continuous process failed.");
    return -1;
  }
  DEBUG_PRINTLINE("GPS continuous process started.");
  return 0;
}

static void GpsStop(void) {
  int32_t gps_ret;

  DEBUG_PRINTLINE("Stop GPS.");
  uLocationGetStop(gGpsHandle);
  gps_ret = uGnssPwrOffBackup(gGpsHandle);
  if (gps_ret == 0) {
    DEBUG_PRINTLINE("GPS Backup mode.\n");
  }
}

//==========================================================================
//==========================================================================
void TestGo(void) {
  //
  if (IsWakeByReset()) {
    PrintLine("Boot from reset.");
  }
  LedSet(true);

  //
  PrintLine("test_gps");

  //
  if (GpsInit() == 0) {
    PrintLine("GPS ready. Wait for locaiton.");
    GpsStart();
  }
  for (;;) {
    // Read input from console
    int key_in = getc(stdin);

    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}