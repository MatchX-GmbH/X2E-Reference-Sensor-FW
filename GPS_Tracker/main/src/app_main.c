//==========================================================================
// Entry point - app_main()
//==========================================================================
#include <stdio.h>

#include "mx_target.h"
#include "task_priority.h"
#include "debug.h"

#include "app_op.h"

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
#endif // ESP_CONSOLE_USB_SERIAL_JTAG
}

//==========================================================================
//==========================================================================
void app_main(void) {
  // Set task priority
  vTaskPrioritySet(NULL, TASK_PRIO_STARTING);

  // Initialization
  InitUsbJtagUart();
  DEBUG_INIT();

  // Start tasks
  AppOpInit();

  // Release the startup task
  vTaskDelete(NULL);
}
