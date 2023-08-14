#ifndef        __BQ27220_H__
#define        __BQ27220_H__

#include <stdbool.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"

#define BQ_SHUNT_RESISTOR       10 // 0.010 ohms * 1000

// Set data into "addr"
#define BQ27220_ADDR            0xAA

// Standard Commands
#define BQ_CNTL                 0x00
#define BQ_AR                   0x02
#define BQ_ARTTE                0x04
#define BQ_TEMP                 0x06
#define BQ_VOLT                 0x08
#define BQ_FLAGS                0x0A
#define BQ_CURRENT              0x0C

#define BQ_RM                   0x10
#define BQ_FCC                  0x12
#define BQ_AI                   0x14
#define BQ_TTE                  0x16
#define BQ_TTF                  0x18
#define BQ_SI                   0x1A
#define BQ_STTE                 0x1C
#define BQ_MLI                  0x1E

#define BQ_MLTTE                0x20
#define BQ_RCC                  0x22
#define BQ_AP                   0x24

#define BQ_INTTEMP              0x28
#define BQ_CYC                  0x2A
#define BQ_SOC                  0x2C
#define BQ_SOH                  0x2E

#define BQ_CV                   0x30
#define BQ_CC                   0x32
#define BQ_BTPD                 0x34
#define BQ_BTPC                 0x36

#define BQ_OS                   0x3A
#define BQ_DC                   0x3C
#define BQ_SUB                  0x3E

#define BQ_MACDATA              0x40

#define BQ_MACDATASUM           0x60
#define BQ_MACDATALEN           0x61

#define BQ_ANACNT               0x79
#define BQ_RAWC                 0x7A
#define BQ_RAWV                 0x7C
#define BQ_RAWT                 0x7E

// Sub Commands
#define BQ_CNTL_STAT            0x0000
#define BQ_DEVICE_NUMBER        0x0001
#define BQ_FW_VERSION           0x0002
#define BQ_HW_VERSION           0x0003

#define BQ_BOARD_OFFSET         0x0009
#define BQ_CC_OFFSET            0x000A
#define BQ_CC_OFFSET_SAVE       0x000B
#define BQ_OCV_CMD              0x000C
#define BQ_BAT_INSERT           0x000D
#define BQ_BAT_REMOVE           0x000E

#define BQ_SET_SNOOZE           0x0013
#define BQ_CLEAR_SNOOZE         0x0014
#define BQ_SET_PROFILE_1        0x0015
#define BQ_SET_PROFILE_2        0x0016
#define BQ_SET_PROFILE_3        0x0017
#define BQ_SET_PROFILE_4        0x0018
#define BQ_SET_PROFILE_5        0x0019
#define BQ_SET_PROFILE_6        0x001A

#define BQ_CAL_TOGGLE           0x002D

#define BQ_SET_SEALED           0x0030

#define BQ_RESET                0x0041

#define BQ_OP_STATUS            0x0054
#define BQ_GAUGE_STATUS         0x0056

#define BQ_EXIT_CAL             0x0080
#define BQ_ENTER_CAL            0x0081

#define BQ_ENTER_CFG_UPDATE     0x0090
#define BQ_EXIT_CFG_UPDATE_REINIT  0x0091
#define BQ_EXIT_CFG_UPDATE      0x0092

#define BQ_RETURN_TO_ROM        0x0F00

// Configuration parameters
#define BQ_CONFIG_CC_GAIN       0x9184      //float

#define BQ_CONFIG_CHG_INH_LO    0x91f5      //int16_t
#define BQ_CONFIG_CHG_INH_HI    0x91f7      //int16_t
#define BQ_CONFIG_CHG_INH_HYST  0x91f9      //int16_t
#define BQ_CONFIG_CHG_CURR      0x91fb      //int16_t
#define BQ_CONFIG_CHG_VOLT      0x91fd      //int16_t

#define BQ_CONFIG_TAPER_CURR    0x9201      //int16_t
#define BQ_CONFIG_OP_CONFIG_A   0x9206      //uint16_t
#define BQ_CONFIG_OP_CONFIG_B   0x9208      //uint16_t
#define BQ_CONFIG_SOC_DELTA     0x920b      //uint8_t
#define BQ_CONFIG_CLK_CTRL      0x920c      //uint8_t
#define BQ_CONFIG_IO_CONFIG     0x920d      //uint8_t
#define BQ_CONFIG_INIT_DIS_SET  0x920e      //int16_t
#define BQ_CONFIG_INIT_CHG_SET  0x9210      //int16_t
#define BQ_CONFIG_DEVICE_TYPE   0x9212      //uint16_t

#define BQ_CONFIG_SLEEP_CURR    0x9217      //int16_t
#define BQ_CONFIG_BUS_LO_TIME   0x9219      //uint8_t
#define BQ_CONFIG_OC_INH_T_LO   0x921a      //int16_t
#define BQ_CONFIG_OC_INH_T_HI   0x921c      //int16_t
#define BQ_CONFIG_SLEEP_V_TIME  0x921e      //uint8_t
#define BQ_CONFIG_SLEEP_C_TIME  0x921f      //uint8_t

#define BQ_CONFIG_DIS_DET_THRES 0x9228      //int16_t
#define BQ_CONFIG_CHG_DET_THRES 0x922a      //int16_t
#define BQ_CONFIG_QUIT_CURR     0x922c      //int16_t
#define BQ_CONFIG_DIS_RELAX_TI  0x922e      //uint16_t

#define BQ_CONFIG_CHG_RELAX_TI  0x9230      //uint8_t
#define BQ_CONFIG_QUIT_RELAX_TI 0x9231      //uint8_t

#define BQ_CONFIG_OT_CHG        0x9232      //int16_t
#define BQ_CONFIG_OT_CHG_TIME   0x9234      //uint8_t
#define BQ_CONFIG_OT_CGH_RECOV  0x9235      //int16_t
#define BQ_CONFIG_OT_DSG        0x9237      //int16_t
#define BQ_CONFIG_OT_DSG_TIME   0x9239      //uint8_t
#define BQ_CONFIG_OT_DSG_RECOV  0x923a      //int16_t
#define BQ_CONFIG_INIT_STBY     0x923c      //int8_t

#define BQ_CONFIG_SYSD_S_VTHRS  0x9240      //int16_t
#define BQ_CONFIG_SYSD_S_VTIME  0x9242      //uint8_t
#define BQ_CONFIG_SYSD_C_VTHRS  0x9243      //int16_t

#define BQ_GG_SMOOTHING_CONFIG  0x9271      //uint8_t
#define BQ_CONFIG_FLAG_CONFIG_A 0x927f      //uint16_t
#define BQ_CONFIG_FLAG_CONFIG_B 0x9281      //uint8_t

#define BQ_CONFIG_BATTERY_ID    0x929a      //uint8_t

// Gas Gauge parameters
#define BQ_GG_CEDVp1_GAUGE_CONF 0x929b      //uint16_t
#define BQ_GG_CEDVp1_FULLC_CAP  0x929d      //int16_t 15 bits
#define BQ_GG_CEDVp1_DESIGN_CAP 0x929f      //int16_t 15 bits
#define BQ_GG_CEDVp1_DESIGN_V   0x92a3      //int16_t 15 bits
#define BQ_GG_CEDVp1_CHG_TERM_V 0x92a5      //int16_t
#define BQ_GG_CEDVp1_EMF        0x92a7      //uint16_t
#define BQ_GG_CEDVp1_C0         0x92a9      //uint16_t
#define BQ_GG_CEDVp1_R0         0x92ab      //uint16_t
#define BQ_GG_CEDVp1_T0         0x92ad      //uint16_t
#define BQ_GG_CEDVp1_R1         0x92af      //uint16_t
#define BQ_GG_CEDVp1_TC         0x92b1      //uint8_t
#define BQ_GG_CEDVp1_C1         0x92b2      //uint8_t
#define BQ_GG_CEDVp1_AGE_FACTOR 0x92b3      //uint8_t
#define BQ_GG_CEDVp1_FIXED_EDV0 0x92b4      //int16_t
#define BQ_GG_CEDVp1_HOLDT_EDV0 0x92b6      //uint8_t
#define BQ_GG_CEDVp1_FIXED_EDV1 0x92b7      //int16_t
#define BQ_GG_CEDVp1_HOLDT_EDV1 0x92b9      //uint8_t
#define BQ_GG_CEDVp1_FIXED_EDV2 0x92ba      //int16_t
#define BQ_GG_CEDVp1_HOLDT_EDV2 0x92bc      //uint8_t

// CNTL_STAT register bit equates
#define BQ_BIT_CS_CCA           0x0020
#define BQ_BIT_CS_BCA           0x0010
#define BQ_BIT_CS_SNOOZE        0x0008
#define BQ_BIT_CS_BAT_ID2       0x0004
#define BQ_BIT_CS_BAT_ID1       0x0002
#define BQ_BIT_CS_BAT_ID0       0x0001
#define BQ_BIT_CS_BAT_ID        (BQ_BIT_CS_BAT_ID2 | BQ_BIT_CS_BAT_ID1 | BQ_BIT_CS_BAT_ID0)

// Gauging Status register bit equates
#define BQ_BIT_GS_VDQ           0x8000
#define BQ_BIT_GS_EDV2          0x4000
#define BQ_BIT_GS_EDV1          0x2000
#define BQ_BIT_GS_RSVD1         0x1000
#define BQ_BIT_GS_RSVD2         0x0800
#define BQ_BIT_GS_FCCX          0x0400
#define BQ_BIT_GS_RSVD3         0x0200
#define BQ_BIT_GS_RSVD4         0x0100
#define BQ_BIT_GS_CF            0x0080
#define BQ_BIT_GS_DSG           0x0040
#define BQ_BIT_GS_EDV           0x0020
#define BQ_BIT_GS_RSVD5         0x0010
#define BQ_BIT_GS_TC            0x0008
#define BQ_BIT_GS_TD            0x0004
#define BQ_BIT_GS_FC            0x0002
#define BQ_BIT_GS_FD            0x0001

// CEDV Gauging Configuration register bit equates
#define BQ_BIT_GC_SME0          0x1000
#define BQ_BIT_GC_IGNORE_SD     0x0800
#define BQ_BIT_GC_FC_FOR_VDQ    0x0400
#define BQ_BIT_GC_FCC_LIMIT     0x0100
#define BQ_BIT_GC_FIXED_EDV0    0x0020
#define BQ_BIT_GC_SC            0x0010
#define BQ_BIT_GC_EDV_CMP       0x0008
#define BQ_BIT_GC_CSYNC         0x0002
#define BQ_BIT_GC_CCT           0x0001

// FLAGS register bit equates
#define BQ_BIT_F_FD             0x8000
#define BQ_BIT_F_OCV_COMP       0x4000
#define BQ_BIT_F_OCV_FAIL       0x2000
#define BQ_BIT_F_SLEEP          0x1000
#define BQ_BIT_F_OTC            0x0800
#define BQ_BIT_F_OTD            0x0400
#define BQ_BIT_F_FC             0x0200
#define BQ_BIT_F_CHGINH         0x0100
#define BQ_BIT_F_TCA            0x0040
#define BQ_BIT_F_OCVGD          0x0020
#define BQ_BIT_F_AUTH_GD        0x0010
#define BQ_BIT_F_BATTPRES       0x0008
#define BQ_BIT_F_TDA            0x0004
#define BQ_BIT_F_SYSDWN         0x0002
#define BQ_BIT_F_DSG            0x0001

// OS register bit equates
#define BQ_BIT_OS_CFGUPDATE     0x0400
#define BQ_BIT_OS_BTPINT        0x0080
#define BQ_BIT_OS_SMTH          0x0040
#define BQ_BIT_OS_INITCOMP      0x0020
#define BQ_BIT_OS_VDQ           0x0010
#define BQ_BIT_OS_EDV2          0x0008
#define BQ_BIT_OS_SEC1          0x0004
#define BQ_BIT_OS_SEC0          0x0002
#define BQ_BIT_OS_CALMD         0x0001

// Operation Config Reg A bit equates
#define BQ_BIT_OCA_TEMPS        0x8000
#define BQ_BIT_OCA_RSVD1        0x4000
#define BQ_BIT_OCA_BATG_POL     0x2000
#define BQ_BIT_OCA_BATG_EN      0x1000
#define BQ_BIT_OCA_RSVD2        0x0800
#define BQ_BIT_OCA_SLEEP        0x0400
#define BQ_BIT_OCA_SLPWAKECHG   0x0200
#define BQ_BIT_OCA_WRTEMP       0x0100
#define BQ_BIT_OCA_BIE          0x0080
#define BQ_BIT_OCA_RSVD3        0x0040
#define BQ_BIT_OCA_BI_PUP_EN    0x0020
#define BQ_BIT_OCA_PFC_CFG1     0x0010
#define BQ_BIT_OCA_PFC_CFG0     0x0008
#define BQ_BIT_OCA_WAKE_EN      0x0004
#define BQ_BIT_OCA_WK_TH1       0x0002
#define BQ_BIT_OCA_WK_TH0       0x0001

// Operation Config Reg B bit equates
#define BQ_BIT_OCB_RSVD1        0x8000
#define BQ_BIT_OCB_RSVD2        0x4000
#define BQ_BIT_OCB_RSVD3        0x2000
#define BQ_BIT_OCB_RSVD4        0x1000
#define BQ_BIT_OCB_DEF_SEAL     0x0800
#define BQ_BIT_OCB_NR           0x0400
#define BQ_BIT_OCB_RSVD5        0x0200
#define BQ_BIT_OCB_RSVD6        0x0100
#define BQ_BIT_OCB_INT_BREM     0x0080
#define BQ_BIT_OCB_INT_BATL     0x0040
#define BQ_BIT_OCB_INT_STATE    0x0020
#define BQ_BIT_OCB_INT_OCV      0x0010
#define BQ_BIT_OCB_RSVD7        0x0008
#define BQ_BIT_OCB_INT_OT       0x0004
#define BQ_BIT_OCB_INT_POL      0x0002
#define BQ_BIT_OCB_INT_FOCV     0x0001

// SOC Flags Reg A bit equates
#define BQ_BIT_SOCFA_TCSETVCT   0x0800
#define BQ_BIT_SOCFA_FCSETVCT   0x0400
#define BQ_BIT_SOCFA_TCCLEARRSOC 0x0080
#define BQ_BIT_SOCFA_TCSETRSOC  0x0040
#define BQ_BIT_SOCFA_TCCLEARV   0x0020
#define BQ_BIT_SOCFA_TCSETV     0x0010
#define BQ_BIT_SOCFA_TDCLEARRSOC 0x0008
#define BQ_BIT_SOCFA_TDSETRSOC  0x0004
#define BQ_BIT_SOCFA_TDCLEARV   0x0002
#define BQ_BIT_SOCFA_TDSETV     0x0001

// SOC Flags Reg B bit equates
#define BQ_BIT_SOCFB_FCCLEARRSOC 0x0080
#define BQ_BIT_SOCFB_FCSETRSOC  0x0040
#define BQ_BIT_SOCFB_FCCLEARV   0x0020
#define BQ_BIT_SOCFB_FCSETV     0x0010
#define BQ_BIT_SOCFB_FDCLEARRSOC 0x0008
#define BQ_BIT_SOCFB_FDSETRSOC  0x0004
#define BQ_BIT_SOCFB_FDCCLEARV  0x0002
#define BQ_BIT_SOCFB_FDSETV     0x0001

// IO Config bit equates
#define BQ_BIT_IOCFG_BtpIntPol  0x0002
#define BQ_BIT_SOCFB_BTpIntEn   0x0001

// Smoothing Config bit equates
#define BQ_BIT_SMOC_SMOOTH_EOC_EN   0x0008
#define BQ_BIT_SMOC_CMEXT       0x0004
#define BQ_BIT_SMOC_VAVG        0x0002
#define BQ_BIT_SMOC_SMEN        0x0001

/**
 * Data structure for BQ27220 data values.
 *
 **/
typedef struct {
  uint16_t cntlReg; /*!< CNTL register */
  int16_t arReg; /*!< AR register */
  uint16_t artteReg; /*!< ARTTE register */
  uint16_t tempReg; /*!< TEMP register */
  uint16_t voltReg; /*!< VOLT register */

  uint16_t flagsReg; /*!< FLAGS register */
  int16_t currentReg; /*!< CURRENT register */
  uint16_t rmReg; /*!< RM register */
  uint16_t fccReg; /*!< FCC register */
  uint16_t aiReg; /*!< AI register */

  uint16_t tteReg; /*!< TTE register */
  uint16_t ttfReg; /*!< TTF register */
  int16_t siReg; /*!< SI register */
  uint16_t stteReg; /*!< STTE register */
  int16_t mliReg; /*!< MLI register */

  uint16_t mltteReg; /*!< MLTTE register */
  uint16_t rawccReg; /*!< RCC register */
  int16_t apReg; /*!< AP register */
  uint16_t intTempReg; /*!< INTTEMP register */
  uint16_t cycReg; /*!< CYC register */

  uint16_t socReg; /*!< SOC register */
  uint16_t sohReg; /*!< SOH register */
  uint16_t cvReg; /*!< CV register */
  uint16_t ccReg; /*!< CC register */
  uint16_t btpdReg; /*!< BTPD register */

  uint16_t btpcReg; /*!< BTPC register */
  uint16_t osReg; /*!< OS register */
  uint16_t dcReg; /*!< DC register */
  uint16_t subReg; /*!< SUB command register */
  char macData[32]; /*!< MAC Data array */
  uint8_t macSumReg; /*!< MAC Data Sum register */

  uint8_t macLenReg; /*!< MAC Data Len register */
  uint8_t anacReg; /*!< Analog Count register */
  uint16_t rawcReg; /*!< RAWC register */
  uint16_t rawvReg; /*!< RAWV register */
  uint16_t rawtReg; /*!< RAWT register */

  uint8_t checksum; /*!< calculated checksum result */

  int16_t shunt_res; /*!< Shunt Resistor value / 1000 */
  char i2c_Bufx[48]; /*!< i2c buffer */
} BQ27220_TypeDef;

esp_err_t i2c_write(uint8_t add_8b, char *buf, uint8_t size, bool single);

esp_err_t i2c_read(uint8_t add_8b, char *buf, uint8_t size, bool single);

/** Write default values for CNTL register and shunt resistor * 1000
 * @param I2c pins
 * @param programming enable EEPROM pin (21V)
 * @return none
 */
void default_init(BQ27220_TypeDef *dataSTR);

uint16_t get_OS_reg(BQ27220_TypeDef *dataSTR);

/** Read all bq registers and put them into the data structure
 * @param pointer to data structure
 * @return i2c error, 0 = no error
 */
int read_registers(BQ27220_TypeDef *dataSTR);

/** Send sub-command and read data and/or result from sub-command
 * @param pointer to data structure
 * @return result and/or data
 */
uint16_t get_sub_cmmd(BQ27220_TypeDef *dataSTR, uint16_t cmmd);

/** Like above, without extra delays
 * @param pointer to data structure
 * @return result and/or data
 */
uint16_t get_sub_cmmd_s(BQ27220_TypeDef *dataSTR, uint16_t cmmd);

//    void change_cfg_OT_chg_time(BQ27220_TypeDef *dataSTR, uint8_t newtime);
void change_ram_1_2_4(BQ27220_TypeDef *dataSTR, uint16_t sub_cmmd, uint32_t value, int qty, bool pre);
//    void change_cfg_6_1(BQ27220_TypeDef *dataSTR);
uint16_t get_cs_len(BQ27220_TypeDef *dataSTR, bool pf);
void exitCfgUpdateReInit(BQ27220_TypeDef *dataSTR);
void exitCfgUpdateExit(BQ27220_TypeDef *dataSTR);
//    void set_reg(BQ27220_TypeDef *dataSTR, uint16_t reg, uint16_t da, int byt);

uint16_t get_reg_2B(BQ27220_TypeDef *dataSTR, uint8_t reg);

void unseal(BQ27220_TypeDef *dataSTR);
void full_access(BQ27220_TypeDef *dataSTR);
void enter_cfg_update(BQ27220_TypeDef *dataSTR);
void seal(BQ27220_TypeDef *dataSTR);
void useProfile_1(BQ27220_TypeDef *dataSTR);
void useProfile_2(BQ27220_TypeDef *dataSTR);
void reset(BQ27220_TypeDef *dataSTR);

/** Send sub-command to get device ID
 * @param pointer to data structure
 * @return sub-command + device id
 */
uint32_t get_dev_id(BQ27220_TypeDef *dataSTR);

/** Send sub-command to get firmware revision
 * @param pointer to data structure
 * @return revision
 */
uint32_t get_fw_rev(BQ27220_TypeDef *dataSTR);

/** Send sub-command to get firmware revision
 * @param pointer to data structure
 * @return revision
 */
uint32_t get_hw_rev(BQ27220_TypeDef *dataSTR);

uint8_t calc_checksum_rx(BQ27220_TypeDef *dataSTR, int length);
uint8_t calc_checksum_tx(BQ27220_TypeDef *dataSTR, int length);

/** Send sub-command to get 32 bytes data
 * @param pointer to data structure
 * @param sub-command
 * @return 32 bytes in macData
 */
uint32_t get_data_32(BQ27220_TypeDef *dataSTR, uint16_t sub_cmmd, int length);

/** Get signed 16 bit value
 * @param pointer to data structure
 * @param sub-command
 * @return 16 bit signed value
 */
uint16_t get_16(BQ27220_TypeDef *dataSTR, uint16_t cmmd);

/** Get unsigned 8 bit value
 * @param pointer to data structure
 * @param sub-command
 * @return 8 bit signed value
 */
uint8_t get_8(BQ27220_TypeDef *dataSTR, uint16_t cmmd);

void set_ntc_as_sensor(BQ27220_TypeDef *dataSTR, bool ntc);

/** Initialize SoC for a new battery
 * @param pointer to data structure
 * @return error, 0 = no error
 */
int new_battery_init(BQ27220_TypeDef *dataSTR);

/** Read all bq EEPROM registers and put them into the data structure
 * @param pointer to data structure
 * @return i2c error, 0 = no error
 */
//    int read_eep_registers(BQ27220_TypeDef *dataSTR);

#endif // MBED_BQ27220
