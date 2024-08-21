#ifndef BOARD_X2E_H
#define BOARD_X2E_H

#include "phhalHw.h"

/******************************************************************
 * Board Pin/Gpio configurations
 ******************************************************************/
#define PHDRIVER_PIN_RESET          12
#define PHDRIVER_PIN_IRQ            14
// #define PHDRIVER_PIN_IFSEL0         27
// #define PHDRIVER_PIN_IFSEL1         22
// #define PHDRIVER_PIN_AD0            4
// #define PHDRIVER_PIN_AD1            17

/******************************************************************
 * PIN Pull-Up/Pull-Down configurations.
 ******************************************************************/
#define PHDRIVER_PIN_RESET_PULL_CFG    PH_DRIVER_PULL_DOWN
#define PHDRIVER_PIN_IRQ_PULL_CFG      PH_DRIVER_PULL_UP

/******************************************************************
 * IRQ & BUSY PIN TRIGGER settings
 ******************************************************************/
#define PIN_IRQ_TRIGGER_TYPE         PH_DRIVER_INTERRUPT_RISINGEDGE

/*****************************************************************
 * Front End Reset logic level settings
 ****************************************************************/
#define PH_DRIVER_SET_HIGH            1          /**< Logic High. */
#define PH_DRIVER_SET_LOW             0          /**< Logic Low. */
#define RESET_POWERDOWN_LEVEL       PH_DRIVER_SET_HIGH
#define RESET_POWERUP_LEVEL         PH_DRIVER_SET_LOW

/*****************************************************************
 * Dummy entries
 * No functionality. To suppress build error in HAL. No pin functionality in SPI Linux BAL.
 *****************************************************************/
#define PHDRIVER_PIN_SSEL                            0xFFFF
#define PHDRIVER_PIN_NSS_PULL_CFG                    PH_DRIVER_PULL_UP

/*****************************************************************
 * STATUS LED Configuration
 ****************************************************************/
#define PHDRIVER_LED_SUCCESS_DELAY      2

#define PHDRIVER_LED_FAILURE_DELAY_MS   250
#define PHDRIVER_LED_FAILURE_FLICKER    4


//==========================================================================
// Board specified functions
//==========================================================================
int8_t BoardConfigureIrq(phhalHw_Rc663_DataParams_t *pHal);

#endif /* BOARD_X2E_H */
