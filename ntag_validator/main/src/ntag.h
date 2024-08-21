//==========================================================================
//==========================================================================
#ifndef INC_NTAG_H
#define INC_NTAG_H

//==========================================================================
//==========================================================================
#include "mx_target.h"

//==========================================================================
//==========================================================================
#define NUM_OF_NTAG_KEY 5
#define NTAG_KEY_SIZE 16
#define NTAG_VERIFY_DATA_SIZE 32

typedef struct {
  bool actionSuccess;
  bool dataVerified;
  bool tamperClosed;
  bool validated;
}NtagActionResult_t;

typedef enum {
  NTAG_ACTION_VALIDATE = 0,
  NTAG_ACTION_INITIALIZE,
  NTAG_ACTION_RESET,
  NTAG_ACTION_CHK_BLANK,
} NtagAction_e;

//==========================================================================
//==========================================================================
int8_t NtagInit(void);
int8_t NtagSetKey(uint8_t aIdx, const uint8_t *aKey, uint16_t aKeyLen);
int8_t NtagSetVerifyData(const uint8_t *aData, uint16_t aLen);
int8_t NtagWait(NtagAction_e aAction, uint32_t aTimeToWait);
NtagActionResult_t *NtagGetResult(void);

//==========================================================================
//==========================================================================
#endif  // INC_NTAG_H