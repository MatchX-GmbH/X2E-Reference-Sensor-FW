//==========================================================================
// X2E Board platform
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
#include "Board_X2E.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Variables
//==========================================================================
static QueueHandle_t gEventQueue = NULL;
static phhalHw_Rc663_DataParams_t *gRc663Hal = NULL;
static TaskHandle_t gMyHandle = NULL;

//==========================================================================
//==========================================================================
static void IRAM_ATTR GpioIrqHandler(void* arg) {
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gEventQueue, &gpio_num, NULL);
}

static void GpioIrqTask(void* arg) {
  uint32_t io_num;
  for (;;) {
    if (xQueueReceive(gEventQueue, &io_num, portMAX_DELAY)) {
      if (gRc663Hal != NULL) {
        if (gRc663Hal->pRFISRCallback != NULL) {
          gRc663Hal->pRFISRCallback(gRc663Hal);
        }
      }
    }
  }
}

//==========================================================================
// Setup IRQ
//==========================================================================
int8_t BoardConfigureIrq(phhalHw_Rc663_DataParams_t* pHal) {
  if (gMyHandle == NULL) {
    // Setup GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = (1ULL << PHDRIVER_PIN_IRQ);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    // create a queue to handle gpio event from isr
    gEventQueue = xQueueCreate(10, sizeof(uint32_t));

    // start gpio task
    xTaskCreate(GpioIrqTask, "NfcGpioIrqTask", 1024, NULL, 10, &gMyHandle);
    if (gMyHandle == NULL) {
      printf("ERROR. Create NfcGpioIrqTask failed.");
      return -1;
    }

    // install gpio isr service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PHDRIVER_PIN_IRQ, GpioIrqHandler, (void*)GPIO_INTR_POSEDGE);
  }

  //
  gRc663Hal = pHal;

  return 0;
}
