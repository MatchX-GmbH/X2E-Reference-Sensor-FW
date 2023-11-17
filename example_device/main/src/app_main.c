//==========================================================================
// Entry point - app_main()
//==========================================================================
#include <stdio.h>

#include "app_op.h"
#include "debug.h"
#include "esp_log.h"
#include "mx_target.h"
#include "nvs_flash.h"
#include "task_priority.h"
#include "cmd_op.h"

//==========================================================================
//==========================================================================

//==========================================================================
// Enable input from USB JTAG UART
//==========================================================================
static void InitUsbJtagUart(void) {
#ifdef ESP_CONSOLE_USB_SERIAL_JTAG
  usb_serial_jtag_driver_config_t usb_serial_jtag_config;
  usb_serial_jtag_config.rx_buffer_size = 1024;
  usb_serial_jtag_config.tx_buffer_size = 1024;

  esp_err_t ret = ESP_OK;
  /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
  if (usb_serial_jtag_driver_install(&usb_serial_jtag_config) != ESP_OK) {
    printf("ERROR. Install USB serial JTAG driver failed.\n");
  } else {
    /* Tell vfs to use usb-serial-jtag driver */
    esp_vfs_usb_serial_jtag_use_driver();
  }
#endif  // ESP_CONSOLE_USB_SERIAL_JTAG
}

//==========================================================================
// Init storage
//==========================================================================
static int InitStorage(void) {
  esp_err_t esp_ret;

  esp_ret = nvs_flash_init();
  if ((esp_ret == ESP_ERR_NVS_NO_FREE_PAGES) || (esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    printf("WARNING. NVS Full or version changed. Erase.\n");
    esp_ret = nvs_flash_erase();
    if (esp_ret != ESP_OK) {
      printf("ERROR. Erase NVS failed. %s\n", esp_err_to_name(esp_ret));
      return -1;
    }
    esp_ret = nvs_flash_init();
  }
  if (esp_ret != ESP_OK) {
    printf("ERROR. Init NVS failed. %s\n", esp_err_to_name(esp_ret));
    return -1;
  }
  DEBUG_PRINTLINE("NVS is ready.");
  return 0;
}

//==========================================================================
//==========================================================================
void app_main(void) {
  // Turn off EPS log
  esp_log_level_set("*", ESP_LOG_NONE);

  // Set task priority
  vTaskPrioritySet(NULL, TASK_PRIO_STARTING);

  // Initialization
  InitUsbJtagUart();
  DEBUG_INIT();
  InitStorage();

  // Start tasks
  AppOpInit();
  CommandOpInit();

  // Release the startup task
  vTaskDelete(NULL);
}
