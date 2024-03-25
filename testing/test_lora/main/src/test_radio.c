//==========================================================================
// Testing LoRa Radio
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
#include "test_radio.h"

#include <stddef.h>
#include <string.h>

#include "app_utils.h"
#include "debug.h"
#include "mx_target.h"
#include "packer.h"
#include "radio/radio.h"

//==========================================================================
//==========================================================================
static bool gTxDone;
static bool gTxError;
static bool gRxDone;
static bool gRxError;
static uint16_t gRxedLen;
static uint8_t gRxedBuf[256];

//==========================================================================
// Radio callbacks
//==========================================================================
void OnTxDone(void) {
  DEBUG_PRINTLINE("%s", __func__);
  gTxDone = true;
}

void OnTxTimeout(void) {
  DEBUG_PRINTLINE("%s", __func__);
  gTxDone = true;
  gTxError = true;
}
void OnRxDone(uint8_t *aPayload, uint16_t aSize, int16_t aRssi, int8_t aSnr) {
  DEBUG_PRINTLINE("%s, size=%u, rssi=%d, snr=%d", __func__, aSize, aRssi, aSnr);
  gRxedLen = aSize;
  if (gRxedLen > sizeof(gRxedBuf)) {
    gRxedLen = sizeof(gRxedBuf);
  }
  memcpy(gRxedBuf, aPayload, gRxedLen);
  gRxDone = true;
}
void OnRxTimeout(void) {
  DEBUG_PRINTLINE("%s", __func__);
  gRxError = true;
}

void OnRxError(void) {
  DEBUG_PRINTLINE("%s", __func__);
  gRxError = true;
}

RadioEvents_t gRadioCallbacks = {
    .TxDone = &OnTxDone,
    .TxTimeout = &OnTxTimeout,
    .RxDone = &OnRxDone,
    .RxTimeout = &OnRxTimeout,
    .RxError = &OnRxError,
    .FhssChangeChannel = NULL,
    .CadDone = NULL,
    .GnssDone = NULL,
    .WifiDone = NULL,
};

//==========================================================================
// Show Radio Status
//==========================================================================
static void ShowRadioStatus(void) {
  RadioState_t status = Radio.GetStatus();
  switch (status) {
    case RF_IDLE:
      PrintLine("status: RF_IDLE");
      break;
    case RF_TX_RUNNING:
      PrintLine("status: RF_TX_RUNNING");
      break;
    case RF_RX_RUNNING:
      PrintLine("status: RF_RX_RUNNING");
      break;
    case RF_CAD:
      PrintLine("status: RF_CAD");
      break;
  }
}
uint32_t SX126xGetDio1PinState(void);

//==========================================================================
// Tx on EU868
//==========================================================================
static void SendEu868(uint16_t aCounter) {
  uint8_t tx_buf[32];
  uint32_t frequency = 868100000;
  int8_t power = 13;
  uint32_t bandwidth = 0;  // 0: 125 kHz, 1: 250 kHz, 2: 500 kHz
  uint32_t datarate = 6;   // SF
  uint8_t coderate = 1;    // 1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8
  uint16_t preamble_len = 8;
  bool iq_inverted = false;
  uint32_t timeout = 4000;

  for (int i = 0; i < sizeof(tx_buf); i++) {
    tx_buf[i] = i + 0x50;
  }
  PackU8(&tx_buf[0], 0xe0);
  PackU16(&tx_buf[2], aCounter);

  if (!Radio.CheckRfFrequency(frequency)) {
    PrintLine("Freq Error. %u", frequency);
    return;
  }

  gTxDone = false;
  gTxError = false;
  Radio.Standby();
  Radio.SetTxConfig(MODEM_LORA, power, 0, bandwidth, datarate, coderate, preamble_len, 0, true, 0, 0, iq_inverted, timeout);
  Radio.SetChannel(frequency);
  Radio.SetMaxPayloadLength(MODEM_LORA, sizeof(tx_buf));
  Radio.Send(tx_buf, sizeof(tx_buf));
}

static void ShowTxHintEu868(void) {
  const char *hint =
      "./reset_lgw_both.sh start && ./test_loragw_hal_rx -k 0 -r 1250 -a 869.1 -b 868.1 -m 0 -d /dev/spidev1.0 -o -215 -n 10";

  PrintLine("Hint: Run this on M2Pro for receiving the data.");
  PrintLine("      %s", hint);
}

//==========================================================================
// Start Rx on EU868
//==========================================================================
static void StartRxEu868(void) {
  uint32_t frequency = 868100000;
  uint32_t bandwidth = 0;  // 0: 125 kHz, 1: 250 kHz, 2: 500 kHz
  uint32_t datarate = 6;   // SF
  uint8_t coderate = 1;    // 1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8
  uint16_t preamble_len = 8;
  bool iq_inverted = true;

  if (!Radio.CheckRfFrequency(frequency)) {
    PrintLine("Freq Error. %u", frequency);
    return;
  }

  gRxDone = false;
  Radio.Standby();
  Radio.SetRxConfig(MODEM_LORA, bandwidth, datarate, coderate, 0, preamble_len, 0xffff, false, 0, true, false, 0, iq_inverted,
                    true);
  Radio.SetChannel(frequency);
  Radio.SetMaxPayloadLength(MODEM_LORA, 240);
  Radio.Rx(0);
}

static void ShowRxHintEu868(void) {
  const char *hint =
      "./reset_lgw_both.sh start && ./test_loragw_hal_tx -m LORA -r 1250 -f 868.1 -b 125 --pa 1 --pwid 15 -d /dev/spidev1.0 -n 10 "
      "-z 32 -s 6 -i";

  PrintLine("Hint: Run this on M2Pro for transmit some data.");
  PrintLine("      %s", hint);
}

//==========================================================================
// Tx on ISM2400
//==========================================================================
static void SendIsm2400(uint16_t aCounter) {
  uint8_t tx_buf[32];
  uint32_t frequency = 2425000000;
  int8_t power = 10;
  uint32_t bandwidth = 2;  // 0: 200 kHz, 1: 400 kHz, 2: 800 kHz, 3: 1600 kHz
  uint32_t datarate = 9;   // SF
  uint8_t coderate = 7;    // 1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8, 5: 4/5 LI, 6: 4/6 LI, 7: 4/8 LI
  uint16_t preamble_len = 8;
  bool iq_inverted = false;
  uint32_t timeout = 4000;

  for (int i = 0; i < sizeof(tx_buf); i++) {
    tx_buf[i] = i + 0x50;
  }
  PackU8(&tx_buf[0], 0xe0);
  PackU16(&tx_buf[2], aCounter);

  if (!Radio.CheckRfFrequency(frequency)) {
    PrintLine("Freq Error. %u", frequency);
    return;
  }

  gTxDone = false;
  gTxError = false;
  Radio.Standby();
  Radio.SetMaxPayloadLength(MODEM_LORA, sizeof(tx_buf));
  Radio.SetChannel(frequency);
  Radio.SetTxConfig(MODEM_LORA, power, 0, bandwidth, datarate, coderate, preamble_len, false, true, false, 0, iq_inverted, timeout);
  Radio.Send(tx_buf, sizeof(tx_buf));
}

static void ShowTxHintIsm2400(void) {
  const char *hint = "./test_sx1280_hal --rx -f 2.425 -b 2 -s 3 -q 0 -l 10";

  PrintLine("Hint: Run this on NEO for receiving the data.");
  PrintLine("      %s", hint);
}

//==========================================================================
// Start Rx on ISM2400
//==========================================================================
static void StartRxIsm2400(void) {
  uint32_t frequency = 2425000000;
  uint32_t bandwidth = 2;  // 0: 200 kHz, 1: 400 kHz, 2: 800 kHz, 3: 1600 kHz
  uint32_t datarate = 9;   // SF
  uint8_t coderate = 7;    // 1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8, 5: 4/5 LI, 6: 4/6 LI, 7: 4/8 LI
  uint16_t preamble_len = 8;
  bool iq_inverted = false;

  if (!Radio.CheckRfFrequency(frequency)) {
    PrintLine("Freq Error. %u", frequency);
    return;
  }

  gRxDone = false;
  Radio.Standby();
  Radio.SetRxConfig(MODEM_LORA, bandwidth, datarate, coderate, 0, preamble_len, 0xffff, false, 0, true, false, 0, iq_inverted,
                    true);
  Radio.SetChannel(frequency);
  Radio.SetMaxPayloadLength(MODEM_LORA, 240);
  Radio.Rx(0);
}

static void ShowRxHintIsm2400(void) {
  const char *hint = "./test_sx1280_hal --tx -f 2.425 -b 2 -s 3 -q 0 -l 10";

  PrintLine("Hint: Run this on NEO for transmit some data.");
  PrintLine("      %s", hint);
}

//==========================================================================
// TX continuous wave
//==========================================================================
void TestRadioCw(TargetRadio_t aTarget) {
  uint16_t cw_length = 10;
  uint32_t freq;

  PrintLine("TestRadioCw start.");

  // Init chip
  if (aTarget == TARGET_SX1280) {
    RadioSelectChip(RADIO_CHIP_SX1280);
    freq = 2425000000;
  } else {
    RadioSelectChip(RADIO_CHIP_SX126X);
    freq = 868100000;
  }
  Radio.Init(&gRadioCallbacks);
  if (RadioIsChipError()) {
    PrintLine("Radio Chip error.");
    return;
  }
  //
  ShowRadioStatus();

  // Start of CW
  uint32_t tick = GetTick();

  gTxDone = false;
  gTxError = false;
  Radio.Standby();
  Radio.SetTxContinuousWave(freq, 10, cw_length);

  //
  PrintLine("Sending continuous wave %uHz for %us.", freq, cw_length);
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    //
    if (gTxDone) {
      PrintLine("Done.");
      break;
    } else if (TickElapsed(tick) >= ((cw_length + 5) * 1000)) {
      PrintLine("ERROR: Timer doesn't triggered.");
      break;
    }
  }

  //
  Radio.Standby();
  PrintLine("TestRadioCw ended.");
}

//==========================================================================
// TX test
//   Run this on M2Pro for rxing
//==========================================================================
typedef enum {
  S_TX_IDLE = 0,
  S_TX_START,
  S_TX_WAIT_DONE,
  S_TX_END,
} TxState_t;

void TestRadioTx(TargetRadio_t aTarget) {
  PrintLine("TestRadioTx start.");

  // Init chip
  if (aTarget == TARGET_SX1280) {
    RadioSelectChip(RADIO_CHIP_SX1280);
  } else {
    RadioSelectChip(RADIO_CHIP_SX126X);
  }
  Radio.Init(&gRadioCallbacks);
  if (RadioIsChipError()) {
    PrintLine("Radio Chip error.");
    return;
  }

  //
  ShowRadioStatus();

  //
  bool abort = false;
  TxState_t state = S_TX_IDLE;
  uint32_t tick_tx = 0;
  uint16_t tx_counter = 0;

  Radio.Standby();
  if (aTarget == TARGET_SX1280) {
    Radio.SetPublicNetwork(true);
    ShowTxHintIsm2400();
  } else {
    Radio.SetPublicNetwork(false);
    ShowTxHintEu868();
  }
  Radio.SetModem(MODEM_LORA);
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    //
    switch (state) {
      case S_TX_IDLE:
        if ((tick_tx == 0) || (TickElapsed(tick_tx) >= 2000)) {
          state = S_TX_START;
        }
        break;
      case S_TX_START:
        if (aTarget == TARGET_SX1280) {
          SendIsm2400(tx_counter);
        } else {
          SendEu868(tx_counter);
        }
        state = S_TX_WAIT_DONE;
        tick_tx = GetTick();
        break;
      case S_TX_WAIT_DONE:
        if (gTxDone) {
          state = S_TX_END;
          if (gTxError) {
            PrintLine("TX ERROR. %u", tx_counter);
          } else {
            PrintLine("TX Done. %u", tx_counter);
          }
        } else if (TickElapsed(tick_tx) >= 10000) {
          PrintLine("ERROR: DIO IRQ and Timer doesn't triggered.");
          state = S_TX_END;
          abort = true;
        }
        break;
      case S_TX_END:
        tx_counter++;
        Radio.Standby();
        tick_tx = GetTick();
        state = S_TX_IDLE;
        break;
      default:
        tick_tx = GetTick();
        state = S_TX_IDLE;
        break;
    }

    //
    if ((tx_counter >= 10) || (abort)) {
      break;
    }
  }
  PrintLine("Total %u frame sent.", tx_counter);

  //
  Radio.Standby();
  PrintLine("TestRadioTx ended.");
}

//==========================================================================
// RX test
//==========================================================================
void TestRadioRx(TargetRadio_t aTarget) {
  PrintLine("TestRadioRx start.");

  // Init chip
  if (aTarget == TARGET_SX1280) {
    RadioSelectChip(RADIO_CHIP_SX1280);
  } else {
    RadioSelectChip(RADIO_CHIP_SX126X);
  }
  Radio.Init(&gRadioCallbacks);
  if (RadioIsChipError()) {
    PrintLine("Radio Chip error.");
    return;
  }

  //
  ShowRadioStatus();

  //
  uint16_t rx_counter = 0;

  Radio.Standby();
  if (aTarget == TARGET_SX1280) {
    Radio.SetPublicNetwork(true);
    Radio.SetModem(MODEM_LORA);
    StartRxIsm2400();
    ShowRxHintIsm2400();
  } else {
    Radio.SetPublicNetwork(false);
    Radio.SetModem(MODEM_LORA);
    StartRxEu868();
    ShowRxHintEu868();
  }

  //
  for (;;) {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    //
    if (gRxDone) {
      gRxDone = false;
      PrintLine("RX Done. len=%u, %u", gRxedLen, rx_counter);
      Hex2String("RX: ", gRxedBuf, gRxedLen);

      rx_counter++;
      if (rx_counter >= 10) {
        break;
      }
    }
  }
  PrintLine("Total %u frame received.", rx_counter);

  //
  Radio.Standby();
  PrintLine("TestRadioTx ended.");
}