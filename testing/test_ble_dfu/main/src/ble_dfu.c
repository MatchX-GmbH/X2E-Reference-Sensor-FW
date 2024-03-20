//==========================================================================
// Ref: Nordic Semi nRF5 SDK v11 BLE DFU Service
//      https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v11.0.0%2Fbledfu_transport_bleservice.html
//==========================================================================
#include "ble_dfu.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app_utils.h"
#include "crc32.h"
#include "debug.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mutex_helper.h"
#include "packer.h"

//==========================================================================
// Defines
//==========================================================================
// Task
#define TASK_STACK_SIZE 2048
#define TASK_MUTEX_TIMEOUT 1000

//
#define MAX_DEVICE_NAME_SIZE 20
#define DEFAULT_DEVICE_NAME "BLE_SERVER"

#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x55
#define SVC_INST_ID 0
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 128
#define PREPARE_BUF_MAX_SIZE 128

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

//
enum {
  IDX_SVC,
  IDX_CHAR_CONTROL,
  IDX_CHAR_VAL_CONTROL,
  IDX_CHAR_CFG_CONTROL,

  IDX_CHAR_DATA,
  IDX_CHAR_VAL_DATA,

  NUM_OF_ATTRIBUTE
};

// For prepare write
struct TPrepareEnv {
  uint8_t *prepare_buf;
  int16_t prepare_len;
};

// Nordic Semi DFU defines
#define OPCODE_START_DFU 0x01
#define OPCODE_INIT_PARAM 0x02
#define OPCODE_RECV_FIRMWARE 0x03
#define OPCODE_VALIDATE_FIRMWARE 0x04
#define OPCODE_ACTIVATE_FIRMWARE 0x05
#define OPCODE_RESET 0x06
#define OPCODE_REPORT_SIZE 0x07
#define OPCODE_REQ_PACKAGE_NOTIFY 0x08
#define OPCODE_RESP_CODE 0x10
#define OPCODE_PACKAGE_NOTIFY 0x11

#define RESP_SUCCESS 1
#define RESP_INVALID_STATE 2
#define RESP_NOT_SUPPORTED 3
#define RESP_INVALUD_SIZE 4
#define RESP_CRC_ERROR 5
#define RESP_OPERATION_FAILED 6

// DFU state
enum {
  S_DFU_IDLE = 0,
  S_WAIT_IMAGE_SIZE,
  S_WAIT_INIT_START,
  S_WAIT_INIT_PARAM,
  S_WAIT_INIT_END,
  S_WAIT_IMAGE_START,
  S_WAIT_IMAGE,
  S_WAIT_VALIDATE,
  S_WAIT_COMPLETE,

  S_ERROR,
  S_END,
};

// Communication timeout (ms)
#define TIMEOUT_NO_DFU_COMM 60000

//==========================================================================
// Variables
//==========================================================================
// Task info
static TaskHandle_t gMyHangle;

// ESP32 OTA
esp_ota_handle_t gUpdateHandle = 0;
const esp_partition_t *gUpdatePartition = NULL;

//
static uint8_t gBleTxingBuf[64];
static uint8_t gBleTxingLen;
static uint8_t gBleControlBuf[16];
static uint8_t gBleControlLen;
static uint16_t gBleControlNotify;
static uint8_t gBleDataBuf[GATTS_DEMO_CHAR_VAL_LEN_MAX];
static uint8_t gBleDataLen;

static uint32_t gFirmwareSize;
static uint32_t gFirmwareCrc;
static uint32_t gFwRxingSize;
static bool gFwRxing;
static uint32_t gFwRxingCrc;
static bool gFwOtaError;

//
static esp_gatt_if_t gCurrentGattIf = 0xff;
static uint16_t gCurrentConnId = 0xffff;
static uint8_t gAdvetConfigDoneFlags = 0;
static char gDeviceName[MAX_DEVICE_NAME_SIZE + 1];
static bool gBleConnected;
static bool gBleDfuServerStarted;

#define MFG_DATA_SIZE 10
static uint8_t gBleMfgData[MFG_DATA_SIZE] = {0xff, 0xff};

uint16_t gGattHandleTable[NUM_OF_ATTRIBUTE];

static struct TPrepareEnv gPrepareWriteEnv;

// Service
static const uint8_t kServiceUuid[ESP_UUID_LEN_128] = {
    // LSB <-------------------------------------------------------------------------------->  MSB
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x30, 0x15, 0x00, 0x00};

// Charateristics
static const uint8_t kCharUuidControl[ESP_UUID_LEN_128] = {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
                                                           0xDE, 0xEF, 0x12, 0x12, 0x31, 0x15, 0x00, 0x00};

static const uint8_t kCharUuidData[ESP_UUID_LEN_128] = {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
                                                        0xDE, 0xEF, 0x12, 0x12, 0x32, 0x15, 0x00, 0x00};

// The length of adv data must be less than 31 bytes
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0400,  // x 1.25 ms =>
    .max_interval = 0x0800,  // x 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = ESP_UUID_LEN_128,
    .p_service_uuid = (uint8_t *)kServiceUuid,
    .flag = (ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x0400,
    .max_interval = 0x0800,
    .appearance = 0x00,
    .manufacturer_len = MFG_DATA_SIZE,
    .p_manufacturer_data = gBleMfgData,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x0800,  // x0.625ms => 1.28s
    .adv_int_max = 0x0800,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
  esp_gatts_cb_t gatts_cb;
  uint16_t gatts_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_handle;
  esp_gatt_srvc_id_t service_id;
  uint16_t char_handle;
  esp_bt_uuid_t char_uuid;
  esp_gatt_perm_t perm;
  esp_gatt_char_prop_t property;
  uint16_t descr_handle;
  esp_bt_uuid_t descr_uuid;
};

// Forward declare of callback function
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {.gatts_cb = gatts_profile_event_handler, .gatts_if = ESP_GATT_IF_NONE},
};

//
static const uint16_t kPrimaryServiceUuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t kCharacterDeclarationUuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t kCharacterClientConfigUuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t kCharAProperities = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t kCharBProperities = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;

static uint8_t kCharConfigControl[2] = {0x00, 0x00};
static uint8_t kCharBufControl[20] = {0};
static uint8_t kCharBufData[20] = {0};

// Full Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t gatt_db[NUM_OF_ATTRIBUTE] = {
    // Service Declaration
    // [IDX_SVC] = {{ESP_GATT_AUTO_RSP},
    //              {ESP_UUID_LEN_16, (uint8_t *)&kPrimaryServiceUuid, ESP_GATT_PERM_READ, sizeof(uint16_t),
    //               sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}},
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP},
                 {ESP_UUID_LEN_16, (uint8_t *)&kPrimaryServiceUuid, ESP_GATT_PERM_READ, sizeof(uint16_t), ESP_UUID_LEN_128,
                  (uint8_t *)kServiceUuid}},

    // Characteristic Declaration
    [IDX_CHAR_CONTROL] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, (uint8_t *)&kCharacterDeclarationUuid, ESP_GATT_PERM_WRITE, sizeof(uint8_t),
                           sizeof(uint8_t), (uint8_t *)&kCharAProperities}},

    // Characteristic Value
    [IDX_CHAR_VAL_CONTROL] = {{ESP_GATT_AUTO_RSP},
                              {ESP_UUID_LEN_128, (uint8_t *)kCharUuidControl, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(kCharBufControl), (uint8_t *)kCharBufControl}},

    // Client Characteristic Configuration Descriptor
    [IDX_CHAR_CFG_CONTROL] = {{ESP_GATT_AUTO_RSP},
                              {ESP_UUID_LEN_16, (uint8_t *)&kCharacterClientConfigUuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               sizeof(uint16_t), sizeof(kCharConfigControl), (uint8_t *)kCharConfigControl}},

    // Characteristic Declaration
    [IDX_CHAR_DATA] = {{ESP_GATT_AUTO_RSP},
                       {ESP_UUID_LEN_16, (uint8_t *)&kCharacterDeclarationUuid, ESP_GATT_PERM_WRITE, sizeof(uint8_t),
                        sizeof(uint8_t), (uint8_t *)&kCharBProperities}},

    // Characteristic Value
    [IDX_CHAR_VAL_DATA] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_128, (uint8_t *)&kCharUuidData, ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX,
                            sizeof(kCharBufData), (uint8_t *)kCharBufData}}};

//==========================================================================
// GAP event handler
//==========================================================================
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  DEBUG_PRINTLINE("BleDfuServer: gap_event_handler %d", event);

  switch (event) {
#ifdef CONFIG_SET_RAW_ADV_DATA
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      gAdvetConfigDoneFlags &= (~ADV_CONFIG_FLAG);
      if (gAdvetConfigDoneFlags == 0) {
        esp_ble_gap_start_advertising(&adv_params);
      }
      break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
      gAdvetConfigDoneFlags &= (~SCAN_RSP_CONFIG_FLAG);
      if (gAdvetConfigDoneFlags == 0) {
        esp_ble_gap_start_advertising(&adv_params);
      }
      break;
#else
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      gAdvetConfigDoneFlags &= (~ADV_CONFIG_FLAG);
      if (gAdvetConfigDoneFlags == 0) {
        esp_ble_gap_start_advertising(&adv_params);
      }
      break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      gAdvetConfigDoneFlags &= (~SCAN_RSP_CONFIG_FLAG);
      if (gAdvetConfigDoneFlags == 0) {
        esp_ble_gap_start_advertising(&adv_params);
      }
      break;
#endif
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      /* advertising start complete event to indicate advertising start
       * successfully or failed */
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        PrintLine("WARN. BleDfuServer advertising start failed");
      } else {
        DEBUG_PRINTLINE("BleDfuServer advertising start successfully");
      }
      break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        PrintLine("ERROR. BleDfuServer advertising stop failed");
      } else {
        DEBUG_PRINTLINE("BleDfuServer stop adv successfully\n");
      }
      break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      DEBUG_PRINTLINE(
          "BleDfuServer update connection params status = %d, min_int = %d, max_int = "
          "%d,conn_int = %d,latency = %d, timeout = %d",
          param->update_conn_params.status, param->update_conn_params.min_int, param->update_conn_params.max_int,
          param->update_conn_params.conn_int, param->update_conn_params.latency, param->update_conn_params.timeout);
      break;
    default:
      break;
  }
}

//==========================================================================
// Prepare write event
//==========================================================================
static void example_prepare_write_event_env(esp_gatt_if_t gatts_if, struct TPrepareEnv *gPrepareWriteEnv,
                                            esp_ble_gatts_cb_param_t *param) {
  DEBUG_PRINTLINE("BleDfuServer prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
  esp_gatt_status_t status = ESP_GATT_OK;
  if (gPrepareWriteEnv->prepare_buf == NULL) {
    gPrepareWriteEnv->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
    gPrepareWriteEnv->prepare_len = 0;
    if (gPrepareWriteEnv->prepare_buf == NULL) {
      PrintLine("ERROR. BleDfuServer, %s, Gatt_server prep no mem", __func__);
      status = ESP_GATT_NO_RESOURCES;
    }
  } else {
    if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
      status = ESP_GATT_INVALID_OFFSET;
    } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
      status = ESP_GATT_INVALID_ATTR_LEN;
    }
  }
  /*send response when param->write.need_rsp is true */
  if (param->write.need_rsp) {
    esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
    if (gatt_rsp != NULL) {
      gatt_rsp->attr_value.len = param->write.len;
      gatt_rsp->attr_value.handle = param->write.handle;
      gatt_rsp->attr_value.offset = param->write.offset;
      gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
      memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
      esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
      if (response_err != ESP_OK) {
        PrintLine("ERROR. BleDfuServer send response error");
      }
      free(gatt_rsp);
    } else {
      PrintLine("ERROR. BleDfuServer, %s, malloc failed", __func__);
    }
  }
  if (status != ESP_GATT_OK) {
    return;
  }
  memcpy(gPrepareWriteEnv->prepare_buf + param->write.offset, param->write.value, param->write.len);
  gPrepareWriteEnv->prepare_len += param->write.len;
}

//==========================================================================
// Write Event
//==========================================================================
static void example_exec_write_event_env(struct TPrepareEnv *gPrepareWriteEnv, esp_ble_gatts_cb_param_t *param) {
  if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && gPrepareWriteEnv->prepare_buf) {
    DEBUG_HEX2STRING("BleDfuServer write event: ", gPrepareWriteEnv->prepare_buf, gPrepareWriteEnv->prepare_len);
  } else {
    DEBUG_PRINTLINE("BleDfuServer ESP_GATT_PREP_WRITE_CANCEL");
  }
  if (gPrepareWriteEnv->prepare_buf) {
    free(gPrepareWriteEnv->prepare_buf);
    gPrepareWriteEnv->prepare_buf = NULL;
  }
  gPrepareWriteEnv->prepare_len = 0;
}

//==========================================================================
// GATT Profile event handler
//==========================================================================
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  // DEBUG_PRINTLINE("gatts_profile_event_handler %d", event);
  switch (event) {
    case ESP_GATTS_REG_EVT: {
      esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(gDeviceName);
      if (set_dev_name_ret) {
        PrintLine("ERROR. BleDfuServer set device name failed, error code = %x", set_dev_name_ret);
      }
#ifdef CONFIG_SET_RAW_ADV_DATA
      esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
      if (raw_adv_ret) {
        PrintLine("ERROR. BleDfuServer config raw adv data failed, error code = %x ", raw_adv_ret);
      }
      gAdvetConfigDoneFlags |= ADV_CONFIG_FLAG;
      esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
      if (raw_scan_ret) {
        PrintLine("ERROR. BleDfuServer config raw scan rsp data failed, error code = %x", raw_scan_ret);
      }
      gAdvetConfigDoneFlags |= SCAN_RSP_CONFIG_FLAG;
#else
      // config adv data
      esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
      if (ret) {
        PrintLine("ERROR. BleDfuServer config adv data failed, error code = %x", ret);
      }
      gAdvetConfigDoneFlags |= ADV_CONFIG_FLAG;
      // config scan response data
      ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
      if (ret) {
        PrintLine("ERROR. BleDfuServer config scan response data failed, error code = %x", ret);
      }
      gAdvetConfigDoneFlags |= SCAN_RSP_CONFIG_FLAG;
#endif
      esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, NUM_OF_ATTRIBUTE, SVC_INST_ID);
      if (create_attr_ret) {
        PrintLine("ERROR. BleDfuServer create attr table failed, error code = %x", create_attr_ret);
      }
    } break;
    case ESP_GATTS_READ_EVT:
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_READ_EVT");
      break;
    case ESP_GATTS_WRITE_EVT:
      if (!param->write.is_prep) {
        if ((gGattHandleTable[IDX_CHAR_CFG_CONTROL] == param->write.handle) && (param->write.len == 2)) {
          // Wrtie to config of DFU Control Point
          gBleControlNotify = param->write.value[1] << 8 | param->write.value[0];
          DEBUG_PRINTLINE("DFU Control Cfg, %04X", gBleControlNotify);

          if (gBleControlNotify == 0x0001) {
            DEBUG_PRINTLINE("BleDfuServer Control notify enable");
          } else if (gBleControlNotify == 0x0002) {
            DEBUG_PRINTLINE("BleDfuServer Control indicate enable");
          } else if (gBleControlNotify == 0x0000) {
            DEBUG_PRINTLINE("BleDfuServer Control notify/indicate disable ");
          } else {
            PrintLine("ERROR. BleDfuServer unknown value on Control nofity.");
            Hex2String("value: ", param->write.value, param->write.len);
          }
        } else if (param->write.handle == gGattHandleTable[IDX_CHAR_VAL_CONTROL]) {
          // Wrtie to DFU Control Point
          if (param->write.len > sizeof(gBleControlBuf)) {
            PrintLine("ERROR. BleDfuServer data too long (%d) for Control end point.", param->write.len);
          } else {
            memcpy(gBleControlBuf, param->write.value, param->write.len);
            gBleControlLen = param->write.len;
          }

        } else if (gGattHandleTable[IDX_CHAR_VAL_DATA] == param->write.handle) {
          // Wrtie to DFU Packet
          if (param->write.len > sizeof(gBleDataBuf)) {
            PrintLine("ERROR. BleDfuServer data too long (%d) for Data end point.", param->write.len);
          } else if (gFwRxing) {
            if ((!gFwOtaError) && (gUpdateHandle > 0)) {
              if (esp_ota_write(gUpdateHandle, (const void *)param->write.value, param->write.len) != ESP_OK) {
                PrintLine("ERROR. BleDfuServer OTA write failed. 0x%08X", gFwRxingSize);
                gFwOtaError = true;
              }
            }
            gFwRxingSize += param->write.len;
            gFwRxingCrc = Crc32Cal(gFwRxingCrc, param->write.value, param->write.len);
          } else {
            memcpy(gBleDataBuf, param->write.value, param->write.len);
            gBleDataLen = param->write.len;
          }
        } else {
          // Unknown
          DEBUG_PRINTLINE("GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
          DEBUG_HEX2STRING("value: ", param->write.value, param->write.len);
        }

        /* send response when param->write.need_rsp is true*/
        if (param->write.need_rsp) {
          esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        }
      } else {
        /* handle prepare write */
        example_prepare_write_event_env(gatts_if, &gPrepareWriteEnv, param);
      }
      break;
    case ESP_GATTS_EXEC_WRITE_EVT:
      // the length of gattc prapare write data must be less than
      // GATTS_DEMO_CHAR_VAL_LEN_MAX.
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_EXEC_WRITE_EVT");
      example_exec_write_event_env(&gPrepareWriteEnv, param);
      break;
    case ESP_GATTS_MTU_EVT:
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
      break;
    case ESP_GATTS_CONF_EVT:
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
      break;
    case ESP_GATTS_START_EVT:
      DEBUG_PRINTLINE("BleDfuServer SERVICE_START_EVT, status %d, service_handle %d", param->start.status,
                      param->start.service_handle);
      break;
    case ESP_GATTS_CONNECT_EVT:
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
      DEBUG_HEX2STRING("remote_bda: ", param->connect.remote_bda, 6);
      gBleControlNotify = 0;
      gBleTxingLen = 0;
      gBleControlLen = 0;
      gBleDataLen = 0;
      gBleConnected = true;
      gCurrentGattIf = gatts_if;
      gCurrentConnId = param->connect.conn_id;
      gFwRxing = false;
      esp_ble_conn_update_params_t conn_params = {0};
      memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      /* For the IOS system, please reference the apple official documents about
       * the ble connection parameters restrictions. */
      conn_params.latency = 0;
      conn_params.max_int = 0x20;  // max_int = 0x20*1.25ms = 40ms
      conn_params.min_int = 0x10;  // min_int = 0x10*1.25ms = 20ms
      conn_params.timeout = 400;   // timeout = 400*10ms = 4000ms
      // start sent the update connection parameters to the peer device.
      esp_ble_gap_update_conn_params(&conn_params);
      break;
    case ESP_GATTS_DISCONNECT_EVT:
      DEBUG_PRINTLINE("BleDfuServer ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
      gBleControlNotify = 0;
      gBleTxingLen = 0;
      gBleControlLen = 0;
      gBleDataLen = 0;
      gBleConnected = false;
      gCurrentGattIf = 0xff;
      gCurrentConnId = 0xffff;
      gFwRxing = false;
      esp_ble_gap_start_advertising(&adv_params);
      break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
      if (param->add_attr_tab.status != ESP_GATT_OK) {
        PrintLine("ERROR. BleDfuServer create attribute table failed, error code=0x%x", param->add_attr_tab.status);
      } else if (param->add_attr_tab.num_handle != NUM_OF_ATTRIBUTE) {
        PrintLine(
            "ERROR. BleDfuServer create attribute table abnormally, num_handle (%d) \
                        doesn't equal to NUM_OF_ATTRIBUTE(%d)",
            param->add_attr_tab.num_handle, NUM_OF_ATTRIBUTE);
      } else {
        DEBUG_PRINTLINE("BleDfuServer create attribute table successfully, the number handle = %d", param->add_attr_tab.num_handle);
        memcpy(gGattHandleTable, param->add_attr_tab.handles, sizeof(gGattHandleTable));
        esp_ble_gatts_start_service(gGattHandleTable[IDX_SVC]);
      }
      break;
    }
    case ESP_GATTS_STOP_EVT:
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    case ESP_GATTS_UNREG_EVT:
    case ESP_GATTS_DELETE_EVT:
    default:
      break;
  }
}

//==========================================================================
// GATT event Handler
//==========================================================================
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  /* If event is register event, store the gatts_if for each profile */
  if (event == ESP_GATTS_REG_EVT) {
    if (param->reg.status == ESP_GATT_OK) {
      heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
    } else {
      PrintLine("ERROR. BleDfuServer reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
      return;
    }
  }
  do {
    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
      /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every
       * profile cb function */
      if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if) {
        if (heart_rate_profile_tab[idx].gatts_cb) {
          heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
        }
      }
    }
  } while (0);
}

//==========================================================================
// Resposes
//==========================================================================
static void PrepareResp(uint8_t aOpCode, uint8_t aRespCode) {
  gBleTxingBuf[0] = OPCODE_RESP_CODE;
  gBleTxingBuf[1] = aOpCode;
  gBleTxingBuf[2] = aRespCode;
  gBleTxingLen = 3;
}

//==========================================================================
// BLE Server Task (Polling input at 50ms)
//==========================================================================
static void BleDfuServerTask(void *pvParameters) {
  int8_t state;
  uint32_t tick_timeout;
  DEBUG_PRINTLINE("BleDfuServerTask Start");

  //
  state = S_DFU_IDLE;
  tick_timeout = GetTick();
  for (;;) {
    //
    if (!gBleDfuServerStarted) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      tick_timeout = GetTick();
      continue;
    } else if (!gBleConnected) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      tick_timeout = GetTick();
      state = S_DFU_IDLE;

      //
      if (gUpdateHandle != 0) {
        esp_ota_abort(gUpdateHandle);
        gUpdateHandle = 0;
      }
    } else {
      vTaskDelay(50 / portTICK_PERIOD_MS);

      if (state != S_WAIT_IMAGE) {
        if (TickElapsed(tick_timeout) >= TIMEOUT_NO_DFU_COMM) {
          PrintLine("ERROR. BleDfuServer communication timeout.");
          esp_ble_gatts_close(gCurrentGattIf, gCurrentConnId);
          vTaskDelay(100 / portTICK_PERIOD_MS);
          state = S_END;
        }
      }
    }

    // Show debug
    if (gBleControlLen > 0) {
      tick_timeout = GetTick();
      DEBUG_HEX2STRING("Control: ", gBleControlBuf, gBleControlLen);
    }
    if (gBleDataLen > 0) {
      tick_timeout = GetTick();
      DEBUG_HEX2STRING("Data: ", gBleDataBuf, gBleDataLen);
    }

    //
    switch (state) {
      case S_DFU_IDLE:
        if (gBleControlLen == 2) {
          if ((gBleControlBuf[0] == OPCODE_START_DFU) && (gBleControlBuf[1] == 0x04)) {
            // Only accept start of application (0x04)

            gUpdatePartition = esp_ota_get_next_update_partition(NULL);
            DEBUG_PRINTLINE("Updating to %s", gUpdatePartition->label);
            if (esp_ota_begin(gUpdatePartition, OTA_WITH_SEQUENTIAL_WRITES, &gUpdateHandle) != ESP_OK) {
              PrepareResp(OPCODE_START_DFU, RESP_OPERATION_FAILED);
              state = S_ERROR;
            } else {
              state = S_WAIT_IMAGE_SIZE;
            }
          } else {
            PrepareResp(OPCODE_START_DFU, RESP_INVALID_STATE);
            state = S_ERROR;
          }

          gBleControlLen = 0;
        }
        break;

      case S_WAIT_IMAGE_SIZE:
        if (gBleDataLen == 12) {
          gFirmwareSize = UnpackU32Le(&gBleDataBuf[8]);
          DEBUG_PRINTLINE("App Size: %d", gFirmwareSize);

          //
          PrepareResp(OPCODE_START_DFU, RESP_SUCCESS);
          gBleDataLen = 0;
          state = S_WAIT_INIT_START;
        }
        break;

      case S_WAIT_INIT_START:
        if (gBleControlLen == 2) {
          if ((gBleControlBuf[0] == OPCODE_INIT_PARAM) && (gBleControlBuf[1] == 0x00)) {
            state = S_WAIT_INIT_PARAM;
          } else {
            PrepareResp(OPCODE_INIT_PARAM, RESP_INVALID_STATE);
            state = S_ERROR;
          }

          gBleControlLen = 0;
        }
        break;

      case S_WAIT_INIT_PARAM:
        if (gBleDataLen > 0) {
          if (gBleDataLen == 16) {
            uint32_t magic_code = UnpackU32Le(&gBleDataBuf[0]);
            if (magic_code == 0x66554433) {
              state = S_WAIT_INIT_END;
              gFirmwareCrc = UnpackU32Le(&gBleDataBuf[4]);

            } else {
              PrepareResp(OPCODE_INIT_PARAM, RESP_NOT_SUPPORTED);
              state = S_ERROR;
            }
          } else {
            PrepareResp(OPCODE_INIT_PARAM, RESP_NOT_SUPPORTED);
            state = S_ERROR;
          }
          gBleDataLen = 0;
        }
        break;

      case S_WAIT_INIT_END:
        if (gBleControlLen == 2) {
          if ((gBleControlBuf[0] == OPCODE_INIT_PARAM) && (gBleControlBuf[1] == 0x01)) {
            PrepareResp(OPCODE_INIT_PARAM, RESP_SUCCESS);
            gFwRxingSize = 0;
            gFwRxingCrc = 0xffffffff;
            gFwOtaError = false;
            gFwRxing = true;
            state = S_WAIT_IMAGE_START;
          } else {
            PrepareResp(OPCODE_INIT_PARAM, RESP_INVALID_STATE);
            state = S_ERROR;
          }
          gBleControlLen = 0;
        }
        break;

      case S_WAIT_IMAGE_START:
        if (gBleControlLen == 1) {
          if (gBleControlBuf[0] == OPCODE_RECV_FIRMWARE) {
            state = S_WAIT_IMAGE;
          } else {
            PrepareResp(OPCODE_RECV_FIRMWARE, RESP_INVALID_STATE);
            state = S_ERROR;
          }
          gBleControlLen = 0;
        }
        break;

      case S_WAIT_IMAGE:
        if (gFwOtaError) {
          PrepareResp(OPCODE_RECV_FIRMWARE, RESP_OPERATION_FAILED);
          state = S_ERROR;
        } else if (gFwRxingSize > gFirmwareSize) {
          PrepareResp(OPCODE_RECV_FIRMWARE, RESP_INVALUD_SIZE);
          state = S_ERROR;
        } else if (gFwRxingSize == gFirmwareSize) {
          DEBUG_PRINTLINE("End of receiving Firmware.");
          PrepareResp(OPCODE_RECV_FIRMWARE, RESP_SUCCESS);
          state = S_WAIT_VALIDATE;
        }
        break;

      case S_WAIT_VALIDATE:
        if (gBleControlLen == 1) {
          if (gBleControlBuf[0] == OPCODE_VALIDATE_FIRMWARE) {
            gFwRxingCrc = Crc32Finalize(gFwRxingCrc);
            if (gFwRxingCrc == gFirmwareCrc) {
              if (esp_ota_end(gUpdateHandle) == ESP_OK) {
                PrepareResp(OPCODE_VALIDATE_FIRMWARE, RESP_SUCCESS);
                state = S_WAIT_COMPLETE;
              } else {
                PrintLine("ERROR. BleDfuServer OTA end failed..");
                PrepareResp(OPCODE_VALIDATE_FIRMWARE, RESP_OPERATION_FAILED);
                state = S_ERROR;
              }
            } else {
              PrintLine("ERROR. BleDfuServer DFU CRC error.");
              PrepareResp(OPCODE_VALIDATE_FIRMWARE, RESP_CRC_ERROR);
              state = S_ERROR;
            }
          }

          gBleControlLen = 0;
        }
        break;

      case S_WAIT_COMPLETE:
        if (gBleControlLen == 1) {
          if (gBleControlBuf[0] == OPCODE_ACTIVATE_FIRMWARE) {
            DEBUG_PRINTLINE("Activate Firmware and reset.");
            esp_ota_set_boot_partition(gUpdatePartition);
            esp_ble_gatts_close(gCurrentGattIf, gCurrentConnId);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            esp_restart();
            state = S_END;
          } else if (gBleControlBuf[0] == OPCODE_ACTIVATE_FIRMWARE) {
            DEBUG_PRINTLINE("Reset.");
            esp_ble_gatts_close(gCurrentGattIf, gCurrentConnId);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            esp_restart();
            state = S_END;
          } else {
            DEBUG_PRINTLINE("Unknown");
            state = S_ERROR;
          }

          gBleControlLen = 0;
        }
        break;

      case S_ERROR:
      case S_END:
      default:
        if (gBleControlLen > 0) gBleControlLen = 0;
        if (gBleDataLen > 0) gBleDataLen = 0;
        break;
    }

    // Send resp
    if (gBleTxingLen) {
      if ((gBleConnected) && (gBleControlNotify != 0)) {
        esp_ble_gatts_send_indicate(gCurrentGattIf, gCurrentConnId, gGattHandleTable[IDX_CHAR_VAL_CONTROL], 3, gBleTxingBuf,
                                    gBleTxingLen);
      }
      gBleTxingLen = 0;
    }
  }

  DEBUG_PRINTLINE("BleDfuServerTask exited.");
}

//==========================================================================
// Init
//==========================================================================
int8_t BleDfuServerInit(void) {
  //
  memset(gDeviceName, 0, sizeof(gDeviceName));
  strncpy(gDeviceName, DEFAULT_DEVICE_NAME, MAX_DEVICE_NAME_SIZE);
  gBleConnected = false;
  gBleDfuServerStarted = false;

  // Init variables
  InitMutex();
  gMyHangle = NULL;

  //
  xTaskCreate(BleDfuServerTask, "BleDfuServerTask", TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &gMyHangle);

  if (gMyHangle == NULL) {
    PrintLine("ERROR. BleDfuServerInit xTaskCreate() failed.");
    return -1;
  }
  DEBUG_PRINTLINE("BleDfuServerTask created.");

  return 0;
}

//==========================================================================
// Start BLE Server
//==========================================================================
int8_t BleDfuServerStart(const char *aName) {
  esp_err_t ret;

  //
  if (gBleDfuServerStarted) {
    BleDfuServerStop();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  // Clear variables
  gBleConnected = false;
  gBleTxingLen = 0;
  gBleControlLen = 0;
  gBleDataLen = 0;
  gBleControlNotify = 0;

  // Set BLE name
  if (aName != NULL) {
    strncpy(gDeviceName, aName, MAX_DEVICE_NAME_SIZE);
  }

  // Init BT
  if (esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
    return -1;
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer %s enable controller failed: %s", __func__, esp_err_to_name(ret));
    return -1;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer %s enable controller failed: %s", __func__, esp_err_to_name(ret));
    return -1;
  }

  // Init Bluedroid
  ret = esp_bluedroid_init();
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer %s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
    return -1;
  }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer %s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
    return -1;
  }

  // Setup GAP and GATT
  ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer gatts register error, error code = %x", ret);
    return -1;
  }

  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer gap register error, error code = %x", ret);
    return -1;
  }

  ret = esp_ble_gatts_app_register(ESP_APP_ID);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer gatts app register error, error code = %x", ret);
    return -1;
  }

  ret = esp_ble_gatt_set_local_mtu(GATTS_DEMO_CHAR_VAL_LEN_MAX);
  if (ret != ESP_OK) {
    PrintLine("ERROR. BleDfuServer set local  MTU failed, error code = %x", ret);
    return -1;
  }

  //
  gBleDfuServerStarted = true;
  return 0;
}

//==========================================================================
// Stop the Server
//==========================================================================
void BleDfuServerStop(void) {
  if (TakeMutex()) {
    gBleConnected = false;
    FreeMutex();
  }

  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  gBleDfuServerStarted = false;
}

//==========================================================================
// Is connected
//==========================================================================
bool BleDfuServerIsConnected(void) { return gBleConnected; }

//==========================================================================
// Is Started
//==========================================================================
bool BleDfuServerIsStarted(void) { return gBleDfuServerStarted; }

//==========================================================================
// Set Mfg data
//==========================================================================
void BleDfuServerSetMfgData(const uint8_t *aData, int16_t aLen) {
  if (aLen > MFG_DATA_SIZE) {
    aLen = MFG_DATA_SIZE;
  }
  memcpy(gBleMfgData, aData, aLen);
  esp_err_t ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
  if (ret) {
    PrintLine("ERROR. BleDfuServer config scan response data failed, error code = %x", ret);
  }
}