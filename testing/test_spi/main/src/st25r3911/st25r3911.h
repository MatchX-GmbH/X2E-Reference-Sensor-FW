/*
******************************************************************************
* GLOBAL DEFINES
******************************************************************************
*/
#define ST25R3911_FDT_NONE                     0x00U    /*!< Value indicating not to perform FDT                        */

#define MS_TO_64FCS(A)                  ((A) * 212U)    /*!< Converts from ms to 64/fc steps                            */
#define MS_FROM_64FCS(A)                ((A) / 212U)    /*!< Converts from 64/fc steps to ms                            */

/* ST25R3911 direct commands */
#define ST25R3911_CMD_SET_DEFAULT              0xC1U    /*!< Puts the chip in default state (same as after power-up)    */
#define ST25R3911_CMD_CLEAR_FIFO               0xC2U    /*!< Stops all activities and clears FIFO                       */
#define ST25R3911_CMD_TRANSMIT_WITH_CRC        0xC4U    /*!< Transmit with CRC                                          */
#define ST25R3911_CMD_TRANSMIT_WITHOUT_CRC     0xC5U    /*!< Transmit without CRC                                       */
#define ST25R3911_CMD_TRANSMIT_REQA            0xC6U    /*!< Transmit REQA                                              */
#define ST25R3911_CMD_TRANSMIT_WUPA            0xC7U    /*!< Transmit WUPA                                              */
#define ST25R3911_CMD_INITIAL_RF_COLLISION     0xC8U    /*!< NFC transmit with Initial RF Collision Avoidance           */
#define ST25R3911_CMD_RESPONSE_RF_COLLISION_N  0xC9U    /*!< NFC transmit with Response RF Collision Avoidance          */
#define ST25R3911_CMD_RESPONSE_RF_COLLISION_0  0xCAU    /*!< NFC transmit with Response RF Collision Avoidance with n=0 */
#define ST25R3911_CMD_NORMAL_NFC_MODE          0xCBU    /*!< NFC switch to normal NFC mode                              */
#define ST25R3911_CMD_ANALOG_PRESET            0xCCU    /*!< Analog Preset                                              */
#define ST25R3911_CMD_MASK_RECEIVE_DATA        0xD0U    /*!< Mask receive data                                          */
#define ST25R3911_CMD_UNMASK_RECEIVE_DATA      0xD1U    /*!< Unmask receive data                                        */
#define ST25R3911_CMD_MEASURE_AMPLITUDE        0xD3U    /*!< Measure singal amplitude on RFI inputs                     */
#define ST25R3911_CMD_SQUELCH                  0xD4U    /*!< Squelch                                                    */
#define ST25R3911_CMD_CLEAR_SQUELCH            0xD5U    /*!< Clear Squelch                                              */
#define ST25R3911_CMD_ADJUST_REGULATORS        0xD6U    /*!< Adjust regulators                                          */
#define ST25R3911_CMD_CALIBRATE_MODULATION     0xD7U    /*!< Calibrate modulation depth                                 */
#define ST25R3911_CMD_CALIBRATE_ANTENNA        0xD8U    /*!< Calibrate antenna                                          */
#define ST25R3911_CMD_MEASURE_PHASE            0xD9U    /*!< Measure phase between RFO and RFI signal                   */
#define ST25R3911_CMD_CLEAR_RSSI               0xDAU    /*!< clear RSSI bits and restart the measurement                */
#define ST25R3911_CMD_TRANSPARENT_MODE         0xDCU    /*!< Transparent mode                                           */
#define ST25R3911_CMD_CALIBRATE_C_SENSOR       0xDDU    /*!< Calibrate the capacitive sensor                            */
#define ST25R3911_CMD_MEASURE_CAPACITANCE      0xDEU    /*!< Measure capacitance                                        */
#define ST25R3911_CMD_MEASURE_VDD              0xDFU    /*!< Measure power supply voltage                               */
#define ST25R3911_CMD_START_GP_TIMER           0xE0U    /*!< Start the general purpose timer                            */
#define ST25R3911_CMD_START_WUP_TIMER          0xE1U    /*!< Start the wake-up timer                                    */
#define ST25R3911_CMD_START_MASK_RECEIVE_TIMER 0xE2U    /*!< Start the mask-receive timer                               */
#define ST25R3911_CMD_START_NO_RESPONSE_TIMER  0xE3U    /*!< Start the no-repsonse timer                                */
#define ST25R3911_CMD_TEST_CLEARA              0xFAU    /*!< Clear Test register                                        */
#define ST25R3911_CMD_TEST_CLEARB              0xFBU    /*!< Clear Test register                                        */
#define ST25R3911_CMD_TEST_ACCESS              0xFCU    /*!< Enable R/W access to the test registers                    */
#define ST25R3911_CMD_LOAD_PPROM               0xFDU    /*!< Load data from the poly fuses to RAM                       */
#define ST25R3911_CMD_FUSE_PPROM               0xFEU    /*!< Fuse poly fuses with data from the RAM                     */


#define ST25R3911_FIFO_DEPTH                   96U      /*!< Depth of FIFO                                              */

#define ST25R3911_THRESHOLD_DO_NOT_SET         0xFFU    /*!< Indicates not to change this Threshold                     */

#define ST25R3911_BR_DO_NOT_SET                0xFFU    /*!< Indicates not to change this Bit Rate                      */
#define ST25R3911_BR_106                       0x00U    /*!< ST25R3911 Bit Rate 106 kbit/s (fc/128)                     */
#define ST25R3911_BR_212                       0x01U    /*!< ST25R3911 Bit Rate 212 kbit/s (fc/64)                      */
#define ST25R3911_BR_424                       0x02U    /*!< ST25R3911 Bit Rate 424 kbit/s (fc/32)                      */
#define ST25R3911_BR_848                       0x03U    /*!< ST25R3911 Bit Rate 848 kbit/s (fc/16)                      */
#define ST25R3911_BR_1695                      0x04U    /*!< ST25R3911 Bit Rate 1696 kbit/s (fc/8)                      */
#define ST25R3911_BR_3390                      0x05U    /*!< ST25R3911 Bit Rate 3390 kbit/s (fc/4)                      */
#define ST25R3911_BR_6780                      0x06U    /*!< ST25R3911 Bit Rate 6780 kbit/s (fc/2)                      */

