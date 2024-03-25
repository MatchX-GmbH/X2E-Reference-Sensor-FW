//==========================================================================
//==========================================================================
#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "app_utils.h"
#include "check_sx1280.h"
#include "check_sx1261.h"
#include "check_nfc.c"
#include "debug.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

//==========================================================================
// Defines
//==========================================================================

//==========================================================================
// Variables
//==========================================================================

//==========================================================================
//==========================================================================
void TestGo(void) {
  esp_err_t ret = ESP_OK;

  // Initialization
  for (;;) {
    CheckSx1280Init();
    CheckSx1261Init();
    CheckNfcInit();

    // All done
    PrintLine("Initialization Done.");
    break;
  };

  if (ret != ESP_OK) {
    PrintLine("ERROR %d, %s", ret, esp_err_to_name(ret));
    return;
  }

  // Test
  for (uint32_t test_count = 1; test_count <= 10; test_count++) {
    bool test_passed = false;
    for (;;) {
      // Basic checking of SX
      if (CheckSx1280Busy() < 0) break;
      if (CheckSx1261Busy() < 0) break;

      //
      if (CheckSx1280() < 0) break;
      if (CheckSx1261() < 0) break;

      //
      if (CheckNfc() < 0) break;

      //
      test_passed = true;
      break;
    }

    if (test_passed) {
      PrintLine("Test passed. %d", test_count);
    } else {
      PrintLine("Test failed. %d", test_count);
    }

    // Sleep between test cycle
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}