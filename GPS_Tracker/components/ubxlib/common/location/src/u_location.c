/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the common location API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_device_shared.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_cell_loc.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_cfg.h"
#include "u_gnss_pos.h"

#include "u_mqtt_common.h"  // Needed by
#include "u_mqtt_client.h"  // u_location_private_cloud_locate.h

#include "u_network.h"
#include "u_network_shared.h"

#include "u_location.h"
#include "u_location_shared.h"

#include "u_location_private_cloud_locate.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

static int32_t startAsync(uDeviceHandle_t devHandle, int32_t desiredRateMs, uLocationType_t type,
                          const uLocationAssist_t *pLocationAssist, const char *pAuthenticationTokenStr,
                          void (*pCallback)(uDeviceHandle_t devHandle, int32_t errorCode, const uLocation_t *pLocation));

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Configure Cell Locate.
static int32_t cellLocConfigure(uDeviceHandle_t cellHandle, int32_t desiredRateMs, const uLocationAssist_t *pLocationAssist,
                                const char *pAuthenticationTokenStr) {
  int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
  int32_t desiredTimeoutSeconds = desiredRateMs / 1000;

  if (pLocationAssist != NULL) {
    uCellLocSetGnssEnable(cellHandle, !pLocationAssist->disableGnss);
    if (pLocationAssist->desiredAccuracyMillimetres >= 0) {
      uCellLocSetDesiredAccuracy(cellHandle, pLocationAssist->desiredAccuracyMillimetres);
    }
    // Use desiredRateMs if it is greater than a second and less than
    // the location assist timeout value
    if ((desiredTimeoutSeconds <= 0) || (pLocationAssist->desiredTimeoutSeconds < desiredTimeoutSeconds)) {
      desiredTimeoutSeconds = pLocationAssist->desiredTimeoutSeconds;
    }
    if (desiredTimeoutSeconds >= 0) {
      uCellLocSetDesiredFixTimeout(cellHandle, desiredTimeoutSeconds);
    }
  }

  if (pAuthenticationTokenStr != NULL) {
    errorCode = uCellLocSetServer(cellHandle, pAuthenticationTokenStr,
    NULL,
                                  NULL);
  }

  return errorCode;
}

// Callback for a non-blocking GNSS position request.
static void gnssPosCallback(uDeviceHandle_t devHandle, int32_t errorCode, int32_t latitudeX1e7, int32_t longitudeX1e7,
                            int32_t altitudeMillimetres, int32_t radiusMillimetres, int32_t speedMillimetresPerSecond, int32_t svs,
                            int64_t timeUtc) {
  uLocationSharedFifoEntry_t *pEntry;
  uLocation_t location;

  if (gULocationMutex != NULL) {

    U_PORT_MUTEX_LOCK(gULocationMutex);

    pEntry = pULocationSharedRequestPop(U_LOCATION_TYPE_GNSS);
    if (pEntry != NULL) {
      if (pEntry->pCallback != NULL) {
        location.type = U_LOCATION_TYPE_GNSS;
        if (errorCode == 0) {
          location.latitudeX1e7 = latitudeX1e7;
          location.longitudeX1e7 = longitudeX1e7;
          location.altitudeMillimetres = altitudeMillimetres;
          location.radiusMillimetres = radiusMillimetres;
          location.speedMillimetresPerSecond = speedMillimetresPerSecond;
          location.svs = svs;
        }
        if (timeUtc >= 0) {
          // Time may be valid even if the error code is non-zero
          location.timeUtc = timeUtc;
        }
        pEntry->pCallback(devHandle, errorCode, &location);
      }
      if (pEntry->desiredRateMs > 0) {
        // Must be in continuous mode: for GNSS we don't need
        // to call startAsync() ('cos either we're one-shot
        // or GNSS is doing the repeated calling into here through
        // streamed position) but we do need to push an entry
        // into the request queue so that we can grab it the
        // next time we're called
        uLocationSharedRequestPush(devHandle, pEntry->desiredRateMs, U_LOCATION_TYPE_GNSS, pEntry->pCallback);
      }
    }
    uPortFree(pEntry);

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }
}

// Callback for a non-blocking cell locate request.
static void cellLocCallback(uDeviceHandle_t devHandle, int32_t errorCode, int32_t latitudeX1e7, int32_t longitudeX1e7,
                            int32_t altitudeMillimetres, int32_t radiusMillimetres, int32_t speedMillimetresPerSecond, int32_t svs,
                            int64_t timeUtc) {
  uLocationSharedFifoEntry_t *pEntry;
  uLocation_t location;

  if (gULocationMutex != NULL) {

    U_PORT_MUTEX_LOCK(gULocationMutex);

    pEntry = pULocationSharedRequestPop(U_LOCATION_TYPE_CLOUD_CELL_LOCATE);
    if (pEntry != NULL) {
      if (pEntry->pCallback != NULL) {
        if (errorCode == 0) {
          location.type = U_LOCATION_TYPE_CLOUD_CELL_LOCATE;
          location.latitudeX1e7 = latitudeX1e7;
          location.longitudeX1e7 = longitudeX1e7;
          location.altitudeMillimetres = altitudeMillimetres;
          location.radiusMillimetres = radiusMillimetres;
          location.speedMillimetresPerSecond = speedMillimetresPerSecond;
          location.svs = svs;
          location.timeUtc = timeUtc;
          pEntry->pCallback(devHandle, errorCode, &location);
        } else {
          // No point in populating the location for
          // Cell Locate if the error code is zero as there's
          // nothing valid to give
          pEntry->pCallback(devHandle, errorCode, NULL);
        }
      }
      if (pEntry->desiredRateMs > 0) {
        // Must be in continuous mode, start again
        startAsync(devHandle, pEntry->desiredRateMs, pEntry->type, NULL, NULL, pEntry->pCallback);
      }
    }
    uPortFree(pEntry);

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }
}

// Start an asynchronous GNSS location attempt.
// gULocationMutex should be locked before this is called.
static int32_t startAsyncGnss(uDeviceHandle_t devHandle, int32_t desiredRateMs,
                              void (*pCallback)(uDeviceHandle_t devHandle, int32_t errorCode, const uLocation_t *pLocation)) {
  uGnssTransportType_t transportType = U_GNSS_TRANSPORT_NONE;
  uGnssTransportHandle_t transportHandle;

  int32_t errorCode = uLocationSharedRequestPush(devHandle, desiredRateMs, U_LOCATION_TYPE_GNSS, pCallback);
  if (errorCode == 0) {
    if (desiredRateMs > 0) {
      errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
      // If the caller wants continuous position, we do
      // that using the GNSS streamed position API.
      uGnssGetTransportHandle(devHandle, &transportType, &transportHandle);
      if (transportType != U_GNSS_TRANSPORT_AT) {
        if (desiredRateMs < U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS) {
          // Switch off NMEA messages to keep the rate up
          uGnssCfgSetProtocolOut(devHandle, U_GNSS_PROTOCOL_NMEA, false);
        }
        errorCode = uGnssPosGetStreamedStart(devHandle, desiredRateMs, gnssPosCallback);
      }
    } else {
      // One-shot mode
      errorCode = uGnssPosGetStart(devHandle, gnssPosCallback);
    }
    if (errorCode != 0) {
      uPortFree(pULocationSharedRequestPop(U_LOCATION_TYPE_GNSS));
    }
  }

  return errorCode;
}

// Start an asynchronous location attempt.
// gULocationMutex should be locked before this is called.
int32_t startAsync(uDeviceHandle_t devHandle, int32_t desiredRateMs, uLocationType_t type, const uLocationAssist_t *pLocationAssist,
                   const char *pAuthenticationTokenStr,
                   void (*pCallback)(uDeviceHandle_t devHandle, int32_t errorCode, const uLocation_t *pLocation)) {
  int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
  int32_t devType = uDeviceGetDeviceType(devHandle);
  if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
    errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
  } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
    if (type == U_LOCATION_TYPE_CLOUD_CELL_LOCATE) {
      errorCode = cellLocConfigure(devHandle, desiredRateMs, pLocationAssist, pAuthenticationTokenStr);
      if (errorCode == 0) {
        errorCode = uLocationSharedRequestPush(devHandle, desiredRateMs, type, pCallback);
        if (errorCode == 0) {
          errorCode = uCellLocGetStart(devHandle, cellLocCallback);
          if (errorCode != 0) {
            uPortFree(pULocationSharedRequestPop(U_LOCATION_TYPE_CLOUD_CELL_LOCATE));
          }
        }
      }
    } else if (type == U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE) {
      errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
      // TODO
    } else if (type == U_LOCATION_TYPE_GNSS) {
      // A GNSS device inside or connected-via a cellular device
      errorCode = startAsyncGnss(devHandle, desiredRateMs, pCallback);
    }
  } else if (devType == (int32_t) U_DEVICE_TYPE_GNSS) {
    // type, pLocationAssist and pAuthenticationTokenStr are
    // irrelevant in this case, we just ask GNSS.
    errorCode = startAsyncGnss(devHandle, desiredRateMs, pCallback);
  }

  return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the current location, blocking version.
int32_t uLocationGet(uDeviceHandle_t devHandle, uLocationType_t type, const uLocationAssist_t *pLocationAssist,
                     const char *pAuthenticationTokenStr, uLocation_t *pLocation,
                     bool (*pKeepGoingCallback)(uDeviceHandle_t)) {
  int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
  uLocation_t location;
  uDeviceHandle_t gnssDeviceHandle;

  if (gULocationMutex != NULL) {
    errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    U_PORT_MUTEX_LOCK(gULocationMutex);

    int32_t devType = uDeviceGetDeviceType(devHandle);
    if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
      errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
    } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
      errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
      location.type = type;
      if (location.type == U_LOCATION_TYPE_CLOUD_CELL_LOCATE) {
        errorCode = cellLocConfigure(devHandle, 0, pLocationAssist, pAuthenticationTokenStr);
        if (errorCode == 0) {
          errorCode = uCellLocGet(devHandle, &(location.latitudeX1e7), &(location.longitudeX1e7), &(location.altitudeMillimetres),
                                  &(location.radiusMillimetres), &(location.speedMillimetresPerSecond), &(location.svs),
                                  &(location.timeUtc), pKeepGoingCallback);
          if (pLocation != NULL) {
            *pLocation = location;
          }
        }
      } else if (location.type == U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        // For Cloud Locate the GNSS network handle is attached to
        // the network data associated with the device handle (and
        // the MQTT client handle is passed in via pLocationAssist)
        gnssDeviceHandle = uNetworkGetDeviceHandle(devHandle, U_NETWORK_TYPE_GNSS);
        if ((pLocationAssist != NULL) && (gnssDeviceHandle != NULL)) {
          errorCode = uLocationPrivateCloudLocate(devHandle, gnssDeviceHandle,
                                                  (uMqttClientContext_t*) pLocationAssist->pMqttClientContext,
                                                  pLocationAssist->svsThreshold, pLocationAssist->cNoThreshold,
                                                  pLocationAssist->multipathIndexLimit,
                                                  pLocationAssist->pseudorangeRmsErrorIndexLimit,
                                                  pLocationAssist->rrlpDataLengthBytes, pLocationAssist->pClientIdStr, &location,
                                                  pKeepGoingCallback);
          if (pLocation != NULL) {
            *pLocation = location;
          }
        }
      } else if (location.type == U_LOCATION_TYPE_GNSS) {
        // A GNSS device inside or connected-via a cellular device
        errorCode = uGnssPosGet(devHandle, &(location.latitudeX1e7), &(location.longitudeX1e7), &(location.altitudeMillimetres),
                                &(location.radiusMillimetres), &(location.speedMillimetresPerSecond), &(location.svs),
                                &(location.timeUtc), pKeepGoingCallback);
        if (pLocation != NULL) {
          pLocation->type = U_LOCATION_TYPE_GNSS;
          *pLocation = location;
        }
      }
    } else if (devType == (int32_t) U_DEVICE_TYPE_GNSS) {
      // type, pLocationAssist and pAuthenticationTokenStr are
      // irrelevant in this case, we just ask GNSS
      errorCode = uGnssPosGet(devHandle, &(location.latitudeX1e7), &(location.longitudeX1e7), &(location.altitudeMillimetres),
                              &(location.radiusMillimetres), &(location.speedMillimetresPerSecond), &(location.svs),
                              &(location.timeUtc), pKeepGoingCallback);
      if (pLocation != NULL) {
        pLocation->type = U_LOCATION_TYPE_GNSS;
        *pLocation = location;
      }
    }

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }

  return errorCode;
}

// Get the current location, non-blocking version.
int32_t uLocationGetStart(uDeviceHandle_t devHandle, uLocationType_t type, const uLocationAssist_t *pLocationAssist,
                          const char *pAuthenticationTokenStr,
                          void (*pCallback)(uDeviceHandle_t devHandle, int32_t errorCode, const uLocation_t *pLocation)) {
  int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

  if (gULocationMutex != NULL) {

    U_PORT_MUTEX_LOCK(gULocationMutex);

    errorCode = startAsync(devHandle, 0, type, pLocationAssist, pAuthenticationTokenStr, pCallback);

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }

  return errorCode;
}

// Get the current location to a callback, continuously.
int32_t uLocationGetContinuousStart(uDeviceHandle_t devHandle, int32_t desiredRateMs, uLocationType_t type,
                                    const uLocationAssist_t *pLocationAssist, const char *pAuthenticationTokenStr,
                                    void (*pCallback)(uDeviceHandle_t devHandle, int32_t errorCode, const uLocation_t *pLocation)) {
  int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

  if (gULocationMutex != NULL) {

    U_PORT_MUTEX_LOCK(gULocationMutex);

    errorCode = startAsync(devHandle, desiredRateMs, type, pLocationAssist, pAuthenticationTokenStr, pCallback);

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }

  return errorCode;
}

// Get the current status of a location establishment attempt.
int32_t uLocationGetStatus(uDeviceHandle_t devHandle) {
  int32_t errorCodeOrStatus = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

  if (gULocationMutex != NULL) {
    errorCodeOrStatus = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    U_PORT_MUTEX_LOCK(gULocationMutex);

    int32_t devType = uDeviceGetDeviceType(devHandle);
    if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
      // Leave as not supported
    } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
      errorCodeOrStatus = uCellLocGetStatus(devHandle);
    } else if (devType == (int32_t) U_DEVICE_TYPE_GNSS) {
      // No way to get it, so return unknown
      errorCodeOrStatus = (int32_t) U_LOCATION_STATUS_UNKNOWN;
    }

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }

  return errorCodeOrStatus;
}

// Cancel a uLocationGetStart()/uLocationGetContinuousStart().
void uLocationGetStop(uDeviceHandle_t devHandle) {
  if (gULocationMutex != NULL) {

    U_PORT_MUTEX_LOCK(gULocationMutex);

    int32_t devType = uDeviceGetDeviceType(devHandle);
    if (devType == (int32_t) U_DEVICE_TYPE_SHORT_RANGE) {
      // Irrelevant
    } else if (devType == (int32_t) U_DEVICE_TYPE_CELL) {
      uCellLocGetStop(devHandle);
      // Also stop these here in case the GNSS device
      // was being acceed via the cellular device
      uGnssPosGetStop(devHandle);
      uGnssPosGetStreamedStop(devHandle);
    } else if (devType == (int32_t) U_DEVICE_TYPE_GNSS) {
      uGnssPosGetStop(devHandle);
      uGnssPosGetStreamedStop(devHandle);
    }

    U_PORT_MUTEX_UNLOCK(gULocationMutex);
  }
}

// End of file
