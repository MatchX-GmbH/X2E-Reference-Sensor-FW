//==========================================================================
// Driver for X2E
//==========================================================================
#include <stdbool.h>
#include <stdio.h>

#include "BoardSelection.h"
#include "NfcCompon_debug.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "i2c_port_1.h"
#include "phDriver.h"

//==========================================================================
// Defines
//==========================================================================
#define I2C_ADDR_RC663 0x28
#define I2C_NUM I2C_PORT1_NUM

//==========================================================================
// Variables
//==========================================================================
static esp_timer_handle_t gNfcTimer = NULL;
static pphDriver_TimerCallBck_t gTimerCallBack = NULL;

//==========================================================================
// Timer
//==========================================================================
static void PeriodicTimerFunc(void *arg) {
  if (gTimerCallBack != NULL) {
    gTimerCallBack();
  }
}

phStatus_t phDriver_TimerStart(phDriver_Timer_Unit_t eTimerUnit, uint32_t dwTimePeriod, pphDriver_TimerCallBck_t pTimerCallBack) {
  if (pTimerCallBack == NULL) {
    // It is a delay
    if (eTimerUnit == PH_DRIVER_TIMER_SECS) {
      NFCCOMPON_PRINTLINE("Delay %ds", dwTimePeriod);
      dwTimePeriod = dwTimePeriod * (1000 / portTICK_PERIOD_MS);
    } else if (eTimerUnit == PH_DRIVER_TIMER_MILLI_SECS) {
      NFCCOMPON_PRINTLINE("Delay %dms (%d)", dwTimePeriod, portTICK_PERIOD_MS);
      dwTimePeriod = dwTimePeriod / portTICK_PERIOD_MS;
    } else {
      NFCCOMPON_PRINTLINE("Delay %dus", dwTimePeriod);
      dwTimePeriod = dwTimePeriod / 1000 / portTICK_PERIOD_MS;
    }
    if (dwTimePeriod == 0) dwTimePeriod = 1;
    vTaskDelay(dwTimePeriod);

    return PH_DRIVER_SUCCESS;
  }

  if (gNfcTimer == NULL) {
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = PeriodicTimerFunc,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "periodic",
    };
    if (esp_timer_create(&periodic_timer_args, &gNfcTimer) != ESP_OK) {
      printf("ERROR. phDriver_TimerStart create timer failed.\n");
      gNfcTimer = NULL;
      gTimerCallBack = NULL;
    } else {
      gTimerCallBack = pTimerCallBack;
    }
  }

  if (gNfcTimer != NULL) {
    // Kick start timer
    if (eTimerUnit == PH_DRIVER_TIMER_SECS) {
      esp_timer_start_periodic(gNfcTimer, dwTimePeriod * 1000000);
      NFCCOMPON_PRINTLINE("Timer started %lus.", dwTimePeriod);
    } else if (eTimerUnit == PH_DRIVER_TIMER_MILLI_SECS) {
      esp_timer_start_periodic(gNfcTimer, dwTimePeriod * 1000);
      NFCCOMPON_PRINTLINE("Timer started %lums.", dwTimePeriod);
    } else {
      esp_timer_start_periodic(gNfcTimer, dwTimePeriod);
      NFCCOMPON_PRINTLINE("Timer started %luus.", dwTimePeriod);
    }
    return PH_DRIVER_SUCCESS;
  } else {
    return PH_DRIVER_FAILURE;
  }
}

phStatus_t phDriver_TimerStop(void) {
  if (gNfcTimer != NULL) {
    esp_timer_stop(gNfcTimer);
    esp_timer_delete(gNfcTimer);
    NFCCOMPON_PRINTLINE("Timer stopped.");
    gNfcTimer = NULL;
    gTimerCallBack = NULL;
  }
  return PH_DRIVER_SUCCESS;
}

//==========================================================================
//==========================================================================
phStatus_t phDriver_PinConfig(uint32_t dwPinNumber, phDriver_Pin_Func_t ePinFunc, phDriver_Pin_Config_t *pPinConfig) {
  // Check GPIO number range
  if (dwPinNumber >= GPIO_NUM_MAX) {
    printf("ERROR. phDriver_PinConfig invalid pin number %d\n", dwPinNumber);
    return PH_DRIVER_ERROR;
  }

  gpio_reset_pin(dwPinNumber);
  if (ePinFunc == PH_DRIVER_PINFUNC_INPUT) {
    NFCCOMPON_PRINTLINE("Config pin %d as input", dwPinNumber);
    gpio_set_direction(dwPinNumber, GPIO_MODE_INPUT);
    if (pPinConfig != NULL) {
      if (pPinConfig->bPullSelect == PH_DRIVER_PULL_UP) {
        gpio_set_pull_mode(dwPinNumber, GPIO_PULLUP_ONLY);
      } else if (pPinConfig->bPullSelect == PH_DRIVER_PULL_DOWN) {
        gpio_set_pull_mode(dwPinNumber, GPIO_PULLDOWN_ONLY);
      }
    }

  } else if (ePinFunc == PH_DRIVER_PINFUNC_OUTPUT) {
    NFCCOMPON_PRINTLINE("Config pin %d as output", dwPinNumber);
    gpio_set_direction(dwPinNumber, GPIO_MODE_OUTPUT);
    if (pPinConfig != NULL) {
      if (pPinConfig->bOutputLogic == 0) {
        gpio_set_level(dwPinNumber, 0);
      } else {
        gpio_set_level(dwPinNumber, 1);
      }
    }

  } else if (ePinFunc == PH_DRIVER_PINFUNC_BIDIR) {
    printf("WARNING: phDriver_PinConfig() Bidirection pin is not supported.\n");
    return PH_DRIVER_ERROR;
  } else if (ePinFunc == PH_DRIVER_PINFUNC_INTERRUPT) {
    printf("WARNING: phDriver_PinConfig() Interrupt pin is not supported.\n");
    return PH_DRIVER_ERROR;
  }

  return PH_DRIVER_SUCCESS;
}

//==========================================================================
//==========================================================================
uint8_t phDriver_PinRead(uint32_t dwPinNumber, phDriver_Pin_Func_t ePinFunc) {
  // Check GPIO number range
  if (dwPinNumber >= GPIO_NUM_MAX) {
    printf("ERROR. phDriver_PinRead invalid pin number %d\n", dwPinNumber);
    return false;
  }

  //
  return gpio_get_level(dwPinNumber);
}

//==========================================================================
//==========================================================================
phStatus_t phDriver_IRQPinRead(uint32_t dwPinNumber) {
  // Check GPIO number range
  if (dwPinNumber >= GPIO_NUM_MAX) {
    printf("ERROR. phDriver_IRQPinRead invalid pin number %d\n", dwPinNumber);
    return false;
  }

  //
  return gpio_get_level(dwPinNumber);
}

//==========================================================================
//==========================================================================
phStatus_t phDriver_IRQPinPoll(uint32_t dwPinNumber, phDriver_Pin_Func_t ePinFunc, phDriver_Interrupt_Config_t eInterruptType) {
  // Check GPIO number range
  if (dwPinNumber >= GPIO_NUM_MAX) {
    printf("ERROR. phDriver_IRQPinPoll invalid pin number %d\n", dwPinNumber);
    return PH_DRIVER_ERROR;
  }

  // Check interrupt type
  if ((eInterruptType != PH_DRIVER_INTERRUPT_RISINGEDGE) && (eInterruptType != PH_DRIVER_INTERRUPT_FALLINGEDGE)) {
    printf("ERROR. phDriver_IRQPinPoll invalid interrupt type %d\n", eInterruptType);
    return PH_DRIVER_ERROR;
  }

  //
  uint8_t bGpioState = 0;

  if (eInterruptType == PH_DRIVER_INTERRUPT_FALLINGEDGE) {
    bGpioState = 1;
  }

  while (phDriver_PinRead(dwPinNumber, ePinFunc) == bGpioState) {
    vTaskDelay(1);
  }

  return PH_DRIVER_SUCCESS;
}

//==========================================================================
//==========================================================================
void phDriver_PinWrite(uint32_t dwPinNumber, uint8_t bValue) {
  // Check GPIO number range
  if (dwPinNumber >= GPIO_NUM_MAX) {
    printf("ERROR. phDriver_PinWrite invalid pin number %d\n", dwPinNumber);
    return;
  }

  if (bValue == 0) {
    gpio_set_level(dwPinNumber, 0);
  } else {
    gpio_set_level(dwPinNumber, 1);
  }
}

//==========================================================================
//==========================================================================
void phDriver_PinClearIntStatus(uint32_t dwPinNumber) { return; }

//==========================================================================
//==========================================================================
void phDriver_EnterCriticalSection(void) { vTaskSuspendAll(); }

void phDriver_ExitCriticalSection(void) { xTaskResumeAll(); }

//==========================================================================
// BAL (Bus Abstraction Layer) Interfaces
//==========================================================================
phStatus_t phbalReg_Exchange(void *pDataParams, uint16_t wOption, uint8_t *pTxBuffer, uint16_t wTxLength, uint16_t wRxBufSize,
                             uint8_t *pRxBuffer, uint16_t *pRxLength) {
  phStatus_t ret = PH_DRIVER_ERROR;

  // I2C only
  if (((phbalReg_Type_t *)pDataParams)->bBalType != PHBAL_REG_TYPE_I2C) {
    printf("ERROR. phbalReg_Exchange unsupported bBalType. I2C only.\n");
    return ret;
  }

  //
  if ((wTxLength == 0) && (wRxBufSize == 0)) {
    printf("ERROR. phbalReg_Exchange nothing to tx/rx\n");
    return ret;
  }

  if ((pTxBuffer == NULL) && (wTxLength > 0)) {
    printf("WARN. phbalReg_Exchange pTxBuffer is NULL.\n");
    wTxLength = 0;
  }
  if ((pRxBuffer == NULL) && (wRxBufSize > 0)) {
    printf("WARN. phbalReg_Exchange pRxBuffer is NULL.\n");
    wRxBufSize = 0;
  }

  //
  // NFCCOMPON_PRINTLINE("phbalReg_Exchange, wRxBufSize=%u", wRxBufSize);
  // if (pTxBuffer != NULL) {
  //   NFCCOMPON_HEX2STRING("TX: ", pTxBuffer, wTxLength);
  // }

  // I2C transaction
  bool link_error = true;
  i2c_cmd_handle_t i2c_link = i2c_cmd_link_create();

  for (;;) {
    // Write register address
    if (i2c_master_start(i2c_link) != ESP_OK) {
      break;
    }
    if (i2c_master_write_byte(i2c_link, (I2C_ADDR_RC663 << 1) | I2C_MASTER_WRITE, true) != ESP_OK) {
      break;
    }

    if (wTxLength > 0) {
      if (i2c_master_write(i2c_link, pTxBuffer, wTxLength, true) != ESP_OK) {
        break;
      }
    }

    if (wRxBufSize == 0) {
      // TX only, end here
      if (i2c_master_stop(i2c_link) != ESP_OK) {
        break;
      }
    } else {
      // Read data
      if (i2c_master_start(i2c_link) != ESP_OK) {
        break;
      }
      if (i2c_master_write_byte(i2c_link, (I2C_ADDR_RC663 << 1) | I2C_MASTER_READ, true) != ESP_OK) {
        break;
      }

      if (i2c_master_read(i2c_link, pRxBuffer, wRxBufSize, I2C_MASTER_LAST_NACK) != ESP_OK) {
        break;
      }
      if (i2c_master_stop(i2c_link) != ESP_OK) {
        break;
      }
    }
    link_error = false;

    // Start the transaction
    if (i2c_master_cmd_begin(I2C_NUM, i2c_link, 200 / portTICK_PERIOD_MS) != ESP_OK) {
      printf("ERROR. phbalReg_Exchange I2C transaction failed.\n");
      break;
    }

    // all done
    if (pRxLength != NULL) {
      *pRxLength = wRxBufSize;
    }
    ret = PH_DRIVER_SUCCESS;
    break;
  }
  i2c_cmd_link_delete(i2c_link);
  vTaskDelay(1 / portTICK_PERIOD_MS);

  if (link_error) {
    printf("ERROR. phbalReg_Exchange I2C link error.\n");
  }

  return ret;
}

phStatus_t phbalReg_Init(void *pDataParams,         /**< [In] Pointer to this layer's parameter structure phbalReg_Type_t. */
                         uint16_t wSizeOfDataParams /**< [In] Size of this layer's parameter structure. */
) {
  if ((pDataParams == NULL) || (sizeof(phbalReg_Type_t) != wSizeOfDataParams)) {
    return (PH_DRIVER_ERROR | PH_COMP_DRIVER);
  }

  ((phbalReg_Type_t *)pDataParams)->wId = PH_COMP_DRIVER;
  ((phbalReg_Type_t *)pDataParams)->bBalType = PHBAL_REG_TYPE_I2C;

  return PH_DRIVER_SUCCESS;
}