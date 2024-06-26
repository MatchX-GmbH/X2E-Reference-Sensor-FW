//==========================================================================
// Testing the LoRa component
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
#include "test_compon.h"

#include <stddef.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "lora_compon.h"
#include "matchx_payload.h"
#include "mx_target.h"
#include "packer.h"

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
// Send data to LoRa
//==========================================================================
static void SendData(void) {
  float voltage0 = 3.21;
  uint8_t buf[8];
  uint8_t *ptr;

  ptr = buf;
  ptr += PackU8(ptr, (MX_DATATYPE_SENSOR | 5));
  ptr += PackU8(ptr, MX_SENSOR_VOLTAGE);
  ptr += PackFloat(ptr, voltage0);
  uint16_t tx_len = ptr - buf;

  Hex2String("Sending ", buf, tx_len);
  LoRaComponSendData(buf, tx_len);
}

//==========================================================================
// Get Data from LoRa
//==========================================================================
static void GetData(void) {
  uint8_t buf[256];
  LoRaRxInfo_t info;

  int32_t rx_len = LoRaComponGetData(buf, sizeof(buf), &info);
  if (rx_len < 0) {
    PrintLine("Rx error.");
  } else {
    PrintLine("Rxed %d bytes. fport=%d, rssi=%d, datarate=%d", rx_len, info.fport, info.rssi, info.datarate);
    Hex2String("  ", buf, rx_len);
  }
}

//==========================================================================
// Test
//==========================================================================
typedef enum {
  S_IDLE = 0,
  S_JOIN_WAIT,
  S_SEND_DATA,
  S_SEND_DATA_WAIT,
  S_WAIT_INTERVAL,
} LoRaState_t;

void TestLoRaComponent(void) {
  uint32_t interval = 120000;

  PrintLine("TestLoRaComponent start. %s", LoRaComponRegionName());

  // Kick start the LoRa component
  const uint8_t pid_hash[32] = {0};
  LoRaComponStart("", pid_hash, false);

  //
  uint32_t tick_lora;
  LoRaState_t state_lora = S_IDLE;
  for (;;) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    //
    switch (state_lora) {
      case S_IDLE:
        if (LoRaComponIsJoined()) {
          PrintLine("Joined.");
          PrintLine("Data sending interval %ds.", interval / 1000);
          tick_lora = GetTick();
          state_lora = S_JOIN_WAIT;
        }
        break;
      case S_JOIN_WAIT:
        if (TickElapsed(tick_lora) >= 5000) {
          state_lora = S_SEND_DATA;
        }
        break;
      case S_SEND_DATA:
        tick_lora = GetTick();
        SendData();
        // Set reporting battery status to external power.
        LoRaComponSetExtPower();
        state_lora = S_SEND_DATA_WAIT;
        break;
      case S_SEND_DATA_WAIT:
        if (LoRaComponIsSendDone()) {
          if (LoRaComponIsSendSuccess()) {
            PrintLine("Send data success.");
          } else {
            PrintLine("Send data failed.");
          }
          state_lora = S_WAIT_INTERVAL;
        }
        break;
      case S_WAIT_INTERVAL:
        if (LoRaComponIsRxReady()) {
          GetData();
        }
        if (TickElapsed(tick_lora) >= interval) {
          state_lora = S_SEND_DATA;
        }
        break;

      default:
        state_lora = S_IDLE;
        break;
    }
    // Check connection
    if ((state_lora != S_IDLE) && (!LoRaComponIsJoined())) {
      PrintLine("Connection lost.");
      state_lora = S_IDLE;
    }
  }
  LoRaComponStop();

  //
  PrintLine("TestLoRaComponent ended.");
}
