
/******************************************************************************
 * @attention
 *
 * COPYRIGHT 2016 STMicroelectronics, all rights reserved
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied,
 * AND SPECIFICALLY DISCLAIMING THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************/

/*
 *      PROJECT:   ST25R3911 firmware
 *      Revision:
 *      LANGUAGE:  ISO C99
 */

/*! \file
 *
 *  \author Ulrich Herrmann
 *
 *  \brief Implementation of ST25R3911 communication.
 *
 */

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "st25r3911_com.h"

#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "rfal_utils.h"
#include "st25r3911.h"

extern spi_device_handle_t gDevNfc;

/*
******************************************************************************
* LOCAL DEFINES
******************************************************************************
*/

#define ST25R3911_WRITE_MODE (0U)     /*!< ST25R3911 SPI Operation Mode: Write                            */
#define ST25R3911_READ_MODE (1U << 6) /*!< ST25R3911 SPI Operation Mode: Read                             */
#define ST25R3911_FIFO_LOAD (2U << 6) /*!< ST25R3911 SPI Operation Mode: FIFO Load                        */
#define ST25R3911_FIFO_READ (0xBFU)   /*!< ST25R3911 SPI Operation Mode: FIFO Read                        */
#define ST25R3911_CMD_MODE (3U << 6)  /*!< ST25R3911 SPI Operation Mode: Direct Command                   */

#define ST25R3911_CMD_LEN (1U) /*!< ST25R3911 CMD length                                           */
#define MAX_HAL_BUFFER_SIZE (ST25R3911_CMD_LEN + ST25R3911_FIFO_DEPTH) /*!< ST25R3911 communication buffer: CMD + FIFO length   */

/*
******************************************************************************
* LOCAL VARIABLES
******************************************************************************
*/
static uint8_t halTxBuffer[MAX_HAL_BUFFER_SIZE] = {0x00};
static uint8_t halRxBuffer[MAX_HAL_BUFFER_SIZE] = {0x00};

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/

static inline void st25r3911CheckFieldSetLED(uint8_t value) {}

//==========================================================================
// GLOBAL FUNCTIONS
//==========================================================================

//==========================================================================
//==========================================================================
void st25r3911ReadRegister(uint8_t reg, uint8_t* value) {
  spi_transaction_t xfer;

  *value = 0;

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911ReadRegister device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = (reg | ST25R3911_READ_MODE);
  halTxBuffer[1] = 0;
  xfer.length = 2 * 8;
  xfer.tx_buffer = halTxBuffer;
  xfer.rx_buffer = halRxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911ReadRegister accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911ReadRegister SPI transmit failed.\n");
  } else {
    // All ok
    *value = halRxBuffer[1];
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ReadMultipleRegisters(uint8_t reg, uint8_t* values, uint8_t length) {
  spi_transaction_t xfer;

  memset(values, 0x00, length);

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911ReadMultipleRegisters device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = (reg | ST25R3911_READ_MODE);
  if (length > (MAX_HAL_BUFFER_SIZE - 1)) {
    printf("ERROR. st25r3911ReadMultipleRegisters data size (%u) too larget.\n", length);
    return;
  }
  xfer.length = (1 + length) * 8;
  xfer.tx_buffer = halTxBuffer;
  xfer.rx_buffer = halRxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911ReadMultipleRegisters accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911ReadMultipleRegisters SPI transmit failed.\n");
  } else {
    // All ok
    memcpy(values, &halRxBuffer[1], length);
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ReadTestRegister(uint8_t reg, uint8_t* value) {
  spi_transaction_t xfer;

  *value = 0;
  
  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911ReadTestRegister device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = ST25R3911_CMD_TEST_ACCESS;
  halTxBuffer[1] = (reg | ST25R3911_READ_MODE);
  xfer.length = 3 * 8;
  xfer.tx_buffer = halTxBuffer;
  xfer.rx_buffer = halRxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911ReadTestRegister accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911ReadTestRegister SPI transmit failed.\n");
  } else {
    // All ok
    *value = halRxBuffer[2];
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911WriteTestRegister(uint8_t reg, uint8_t value) {
  spi_transaction_t xfer;
  
  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911WriteTestRegister device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = ST25R3911_CMD_TEST_ACCESS;
  halTxBuffer[1] = (reg | ST25R3911_WRITE_MODE);
  halTxBuffer[2] = value;
  xfer.length = 3 * 8;
  xfer.tx_buffer = halTxBuffer;
  xfer.rx_buffer = halRxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911WriteTestRegister accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911WriteTestRegister SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911WriteRegister(uint8_t reg, uint8_t value) {
  spi_transaction_t xfer;
  
  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911WriteRegister device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = (reg | ST25R3911_WRITE_MODE);
  halTxBuffer[1] = value;
  xfer.length = 2 * 8;
  xfer.tx_buffer = halTxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911WriteRegister accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911WriteRegister SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ClrRegisterBits(uint8_t reg, uint8_t clr_mask) {
  uint8_t tmp;

  st25r3911ReadRegister(reg, &tmp);
  tmp &= ~clr_mask;
  st25r3911WriteRegister(reg, tmp);

  return;
}

//==========================================================================
//==========================================================================
void st25r3911SetRegisterBits(uint8_t reg, uint8_t set_mask) {
  uint8_t tmp;

  st25r3911ReadRegister(reg, &tmp);
  tmp |= set_mask;
  st25r3911WriteRegister(reg, tmp);

  return;
}

//==========================================================================
//==========================================================================
void st25r3911ChangeRegisterBits(uint8_t reg, uint8_t valueMask, uint8_t value) {
  st25r3911ModifyRegister(reg, valueMask, (valueMask & value));
}

//==========================================================================
//==========================================================================
void st25r3911ModifyRegister(uint8_t reg, uint8_t clr_mask, uint8_t set_mask) {
  uint8_t tmp;

  st25r3911ReadRegister(reg, &tmp);

  /* mask out the bits we don't want to change */
  tmp &= ~clr_mask;
  /* set the new value */
  tmp |= set_mask;
  st25r3911WriteRegister(reg, tmp);

  return;
}

//==========================================================================
//==========================================================================
void st25r3911ChangeTestRegisterBits(uint8_t reg, uint8_t valueMask, uint8_t value) {
  uint8_t rdVal;
  uint8_t wrVal;

  /* Read current reg value */
  st25r3911ReadTestRegister(reg, &rdVal);

  /* Compute new value */
  wrVal = (rdVal & ~valueMask);
  wrVal |= (value & valueMask);

  /* Write new reg value */
  st25r3911WriteTestRegister(reg, wrVal);

  return;
}

//==========================================================================
//==========================================================================
void st25r3911WriteMultipleRegisters(uint8_t reg, const uint8_t* values, uint8_t length) {
  spi_transaction_t xfer;

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911WriteMultipleRegisters device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = (reg | ST25R3911_WRITE_MODE);
  if (length > (MAX_HAL_BUFFER_SIZE - 1)) {
    printf("ERROR. st25r3911WriteMultipleRegisters data size (%u) too larget.\n", length);
    return;
  }
  if ((values != NULL) && (length > 0)) {
    memcpy(&halTxBuffer[1], values, length);
  }
  xfer.length = (1 + length) * 8;
  xfer.tx_buffer = halTxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911WriteMultipleRegisters accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911WriteMultipleRegisters SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911WriteFifo(const uint8_t* values, uint8_t length) {
  spi_transaction_t xfer;

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911WriteFifo device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = ST25R3911_FIFO_LOAD;
  if (length > (MAX_HAL_BUFFER_SIZE - 1)) {
    printf("ERROR. st25r3911WriteFifo data size (%u) too larget.\n", length);
    return;
  }
  if ((values != NULL) && (length > 0)) {
    memcpy(&halTxBuffer[1], values, length);
  }
  xfer.length = (1 + length) * 8;
  xfer.tx_buffer = halTxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911WriteFifo accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911WriteFifo SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ReadFifo(uint8_t* buf, uint8_t length) {
  spi_transaction_t xfer;

  memset(buf, 0x00, length);

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911ReadFifo device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = ST25R3911_FIFO_READ;
  if (length > (MAX_HAL_BUFFER_SIZE - 1)) {
    printf("ERROR. st25r3911ReadFifo data size (%u) too larget.\n", length);
    return;
  }
  xfer.length = (1 + length) * 8;
  xfer.tx_buffer = halTxBuffer;
  xfer.rx_buffer = halRxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911ReadFifo accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911ReadFifo SPI transmit failed.\n");
  } else {
    // All ok
    memcpy(buf, &halRxBuffer[1], length);
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ExecuteCommand(uint8_t cmd) {
  spi_transaction_t xfer;
  
  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911ExecuteCommand device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  halTxBuffer[0] = (cmd | ST25R3911_CMD_MODE);
  xfer.length = 1 * 8;
  xfer.tx_buffer = halTxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911ExecuteCommand accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911ExecuteCommand SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
void st25r3911ExecuteCommands(const uint8_t* cmds, uint8_t length) {
  spi_transaction_t xfer;

  if (gDevNfc == NULL) {
    printf("ERROR. st25r3911WriteFifo device not registered.\n");
    return;
  }

  // Set up transfer info
  memset(&halTxBuffer, 0, sizeof(halTxBuffer));
  memset(&halRxBuffer, 0, sizeof(halRxBuffer));
  memset(&xfer, 0, sizeof(xfer));
  if (length > (MAX_HAL_BUFFER_SIZE)) {
    printf("ERROR. st25r3911WriteFifo data size (%u) too larget.\n", length);
    return;
  }
  if ((cmds != NULL) && (length > 0)) {
    memcpy(halTxBuffer, cmds, length);
  }
  xfer.length = length * 8;
  xfer.tx_buffer = halTxBuffer;

  // start
  if (spi_device_acquire_bus(gDevNfc, portMAX_DELAY) != ESP_OK) {
    printf("ERROR. st25r3911WriteFifo accuire SPI bus failed.\n");
  } else if (spi_device_polling_transmit(gDevNfc, &xfer) != ESP_OK) {
    printf("ERROR. st25r3911WriteFifo SPI transmit failed.\n");
  } else {
    // All ok
  }
  spi_device_release_bus(gDevNfc);
}

//==========================================================================
//==========================================================================
bool st25r3911IsRegValid(uint8_t reg) {
  if ((!(((int16_t)reg >= (int16_t)ST25R3911_REG_IO_CONF1) && (reg <= ST25R3911_REG_CAPACITANCE_MEASURE_RESULT))) &&
      (reg != ST25R3911_REG_IC_IDENTITY)) {
    return false;
  }
  return true;
}
