//==========================================================================
// Entry point - app_main()
//==========================================================================
#include <stdio.h>
#include <string.h>

#include "app_op.h"
#include "debug.h"
#include "esp_log.h"
#include "mx_target.h"
#include "nvs_flash.h"
#include "task_priority.h"
#include "cmd_op.h"
#include "ble_dfu.h"
#include "esp_ota_ops.h"

//==========================================================================
//==========================================================================

//==========================================================================
// Enable input from USB JTAG UART
//==========================================================================
static void InitUsbJtagUart(void) {
#ifdef ESP_CONSOLE_USB_SERIAL_JTAG
  usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT;

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
// OTA
//==========================================================================
static bool diagnostic(void) {
  // A dummy diagnostics, always success.
  printf("Diagnostics ...\n");
  vTaskDelay(100 / portTICK_PERIOD_MS);

  return true;
}

static void CheckOta(void) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  esp_app_desc_t running_app_info;

  if (esp_ota_get_partition_description(running, &running_app_info) != ESP_OK) {
    memset(&running_app_info, 0, sizeof(esp_app_desc_t));
    strncpy(running_app_info.project_name, "UNKNOWN", sizeof(running_app_info.project_name));
  }
  printf("Running partition %s, size=%u, %s (%s)\n", running->label, running->size, running_app_info.project_name,
         running_app_info.version);

  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    DEBUG_PRINTLINE("OTA state=%d", ota_state);
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      // run diagnostic function ...
      bool diagnostic_is_ok = diagnostic();
      if (diagnostic_is_ok) {
        printf("OTA diagnostics completed successfully!\n");
        esp_ota_mark_app_valid_cancel_rollback();
      } else {
        printf("OTA diagnostics failed! Start rollback.\n");
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
    }
  }
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
  CheckOta();
  InitStorage();

  // Start tasks
  AppOpInit();
  CommandOpInit();
  BleDfuServerInit();

  // Release the startup task
  vTaskDelete(NULL);
}
