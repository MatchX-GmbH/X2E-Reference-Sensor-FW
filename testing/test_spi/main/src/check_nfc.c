//==========================================================================
// Check SX1261
//==========================================================================
#include "check_nfc.h"

#include <stdio.h>
#include <string.h>

#include "app_utils.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "mx_target.h"
#include "st25r3911/nfc-gpio.h"
#include "st25r3911/st25r3911.h"
#include "st25r3911/st25r3911_com.h"

//==========================================================================
// Defines
//==========================================================================
#define SPIHOST SPI3_HOST

//==========================================================================
// Variables
//==========================================================================
spi_device_handle_t gDevNfc = NULL;

//==========================================================================
// Init
//==========================================================================
void CheckNfcInit(void) {
#if defined(CONFIG_MATCHX_TARGET_X2E_REF)
  PrintLine("No NFC at %s. CheckNfcInit() skipped.", MxTargetName());
  return;
#endif

  // Register device
  if (gDevNfc == NULL) {
    // Init GPIOs
    gpio_reset_pin(NFC_CS);
    gpio_set_direction(NFC_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(NFC_CS, 1);

    gpio_reset_pin(NFC_INT);
    gpio_set_direction(NFC_INT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(NFC_INT, GPIO_FLOATING);

    spi_device_interface_config_t nfc_cfg;
    memset(&nfc_cfg, 0, sizeof(nfc_cfg));
    nfc_cfg.mode = 0;  // SPI mode 0
    nfc_cfg.clock_speed_hz = SPI_MASTER_FREQ_10M;
    nfc_cfg.spics_io_num = NFC_CS;
    nfc_cfg.flags = 0;
    nfc_cfg.queue_size = 20;

    esp_err_t ret = spi_bus_add_device(SPIHOST, &nfc_cfg, &gDevNfc);
    if (ret != ESP_OK) {
      printf("ERROR. SPI add NFC device failed.\n");
    }
  }
}

//==========================================================================
// Check NFC chip
//==========================================================================
int CheckNfc(void) {
#if defined(CONFIG_MATCHX_TARGET_X2E_REF)
  PrintLine("No NFC at %s. CheckNfc() skipped.", MxTargetName());
  return 0;
#endif

  /* Execute a Set Default on ST25R3911 */
  st25r3911ExecuteCommand(ST25R3911_CMD_SET_DEFAULT);

  /* Set Registers which are not affected by Set default command to default value */
  st25r3911WriteRegister(ST25R3911_REG_OP_CONTROL, 0x00);
  st25r3911WriteRegister(ST25R3911_REG_IO_CONF1, ST25R3911_REG_IO_CONF1_osc);
  st25r3911WriteRegister(ST25R3911_REG_IO_CONF2, 0x00);

  /* Enable pull downs on miso line */
  st25r3911ModifyRegister(ST25R3911_REG_IO_CONF2, 0x00, ST25R3911_REG_IO_CONF2_miso_pd1 | ST25R3911_REG_IO_CONF2_miso_pd2);

  // Check ID
  uint8_t chip_id;

  chip_id = 0;
  st25r3911ReadRegister(ST25R3911_REG_IC_IDENTITY, &chip_id);

  /* Check if IC Identity Register contains ST25R3911's IC type code */
  if ((chip_id & ST25R3911_REG_IC_IDENTITY_mask_ic_type) != ST25R3911_REG_IC_IDENTITY_ic_type) {
    PrintLine("NFC: Incorrect ID. 0x%02X", chip_id);
    return -1;
  }

  uint8_t rev = (chip_id & ST25R3911_REG_IC_IDENTITY_mask_ic_rev);
  const char *rev_str = "Unknown";
  switch (rev) {
    case 2:
      rev_str = "silicon r3.1";
      break;
    case 3:
      rev_str = "silicon r3.3";
      break;
    case 4:
      rev_str = "silicon r4.0";
      break;
    case 5:
      rev_str = "silicon r4.1";
      break;
  }

  PrintLine("NFC: Read ID=0x%02X, rev %s", chip_id, rev_str);

  //
  PrintLine("NFC: Test success.");
  return 0;
}
