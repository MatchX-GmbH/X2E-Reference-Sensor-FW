//==========================================================================
// Air quality
//==========================================================================
//  Copyright (c) MatchX GmbH.  All rights reserved.
//==========================================================================
// Naming conventions
// ~~~~~~~~~~~~~~~~~~
//                Class : Leading C
//               Struct : Leading T
//       typedef Struct : tailing _t
//             Constant : Leading k
//      Global Variable : Leading g
//    Function argument : Leading a
//       Local Variable : All lower case
//==========================================================================
#include "air_quality.h"

#include "debug.h"
#include "iaq_2nd_gen_ulp.h"
#include "zmod4410_config_iaq2_ulp.h"
#include "zmod4xxx.h"
#include "zmod4xxx_cleaning.h"
#include "zmod4xxx_hal.h"

//==========================================================================
//==========================================================================
static uint8_t gAdcResult[ZMOD4410_ADC_DATA_LEN];
static uint8_t gProdData[ZMOD4410_PROD_DATA_LEN];
static iaq_2nd_gen_ulp_handle_t gAlgoHandle;
static iaq_2nd_gen_ulp_inputs_t gAlgoInput;
static zmod4xxx_dev_t gZmodDev;

//==========================================================================
// Init
//==========================================================================
int8_t AirQualityInit(void) {
  int8_t ret;

  deinit_hardware();

  // Init
  ret = init_hardware(&gZmodDev);
  if (ret) {
    printf("ERROR: ZMOD4410 hw init failed, ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

  // Set device variables
  gZmodDev.i2c_addr = ZMOD4410_I2C_ADDR;
  gZmodDev.pid = ZMOD4410_PID;
  gZmodDev.init_conf = &zmod_iaq2_ulp_sensor_cfg[INIT];
  gZmodDev.meas_conf = &zmod_iaq2_ulp_sensor_cfg[MEASUREMENT];
  gZmodDev.prod_data = gProdData;

  // Read product ID and configuration parameters.
  ret = zmod4xxx_read_sensor_info(&gZmodDev);
  if (ret) {
    printf("ERROR: ZMOD4410 read info failed, ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

  // Retrieve sensors unique tracking number and individual trimming information.
  // Provide this information when requesting support from Renesas.
  uint8_t track_number[ZMOD4XXX_LEN_TRACKING];

  ret = zmod4xxx_read_tracking_number(&gZmodDev, track_number);
  if (ret) {
    printf("ERROR: ZMOD4410 reading tracking number failed, ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

#if (DEBUG)
  printf("[DEBUG] ZMOD4410 tracking number: x0000");
  for (int i = 0; i < sizeof(track_number); i++) {
    printf("%02X", track_number[i]);
  }
  printf("\n");
  printf("[DEBUG] ZMOD4410 trimming data: ");
  for (int i = 0; i < sizeof(gProdData); i++) {
    printf(" %i", gProdData[i]);
  }
  printf("\n");
#endif

  // Start the cleaning procedure. Check the Programming Manual on indications
  // of usage. IMPORTANT NOTE: The cleaning procedure can be run only once
  // during the modules lifetime and takes 1 minute (blocking).
  printf("ZMOD4410 starting cleaning procedure. This might take up to 1 min ...\n");
  ret = zmod4xxx_cleaning_run(&gZmodDev);
  if (ERROR_CLEANING == ret) {
    DEBUG_PRINTLINE("ZMOD4410 skipping cleaning procedure. It has already been performed!");
  } else if (ret) {
    printf("ERROR: ZMOD4410 cleaning procedure failed. ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

  // Determine calibration parameters and configure measurement.
  DEBUG_PRINTLINE("ZMOD4410 prepare sensor.");
  ret = zmod4xxx_prepare_sensor(&gZmodDev);
  if (ret) {
    printf("ERROR: ZMOD4410 prepare failed. ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

  /*
   * One-time initialization of the algorithm. Handle passed to calculation
   * function.
   */
  DEBUG_PRINTLINE("ZMOD4410 init algorithm.");
  ret = init_iaq_2nd_gen_ulp(&gAlgoHandle);
  if (ret) {
    printf("ERROR: ZMOD4410 init algorithm failed. ret=%d\n", ret);
    deinit_hardware();
    return -1;
  }

  //
  return 0;
}

//==========================================================================
// Kick start measurement
//==========================================================================
int8_t AirQualityStartMeas(void) {
  int8_t ret;

  // Kick start the measurement
  ret = zmod4xxx_start_measurement(&gZmodDev);
  if (ret) {
    printf("ERROR: ZMOD4410 start measurement failed. ret=%d\n", ret);
    return -1;
  }

  return 0;
}

//==========================================================================
// Check measure is done
//==========================================================================
bool AirQualityIsMeasDone(void) {
  int8_t ret;
  uint8_t zmod4xxx_status;

  // Read status
  ret = zmod4xxx_read_status(&gZmodDev, &zmod4xxx_status);
  if (ret) {
    printf("ERROR: ZMOD4410 read status failed. ret=%d\n", ret);
    return false;
  }
  if (zmod4xxx_status & STATUS_SEQUENCER_RUNNING_MASK) {
    return false;
  }

  return true;
}

//==========================================================================
/// Start measure and get result (will blocked for 3s)
//==========================================================================
int8_t AirQualityGetResult(AirQualityResult_t *aResult, float aTemp, float aHumi) {
  int8_t ret;
  iaq_2nd_gen_ulp_results_t algo_results;
  uint8_t zmod4xxx_status;

  //
  aResult->valid = false;
  aResult->warmUp = false;
  aResult->eco2 = NAN;
  aResult->etoh = NAN;
  aResult->iaq = NAN;
  aResult->tvoc = NAN;

  // Verify completion of measurement sequence
  ret = zmod4xxx_read_status(&gZmodDev, &zmod4xxx_status);
  if (ret) {
    printf("ERROR: ZMOD4410 read status failed. ret=%d\n", ret);
    return -1;
  }

  // Check if measurement is running.
  if (zmod4xxx_status & STATUS_SEQUENCER_RUNNING_MASK) {
    // Check if reset during measurement occured.
    ret = zmod4xxx_check_error_event(&gZmodDev);
    switch (ret) {
      case ERROR_POR_EVENT:
        printf("ERROR: ZMOD4410 measurement completion fault. Unexpected sensor reset.\n");
        break;
      case ZMOD4XXX_OK:
        printf("ERROR: ZMOD4410 measurement completion fault. Wrong sensor setup.\n");
        break;
      default:
        printf("ERROR: ZMOD4410 read status register failed. ret=%d\n", ret);
        break;
    }
    return -1;
  }

  // Read sensor ADC output.
  ret = zmod4xxx_read_adc_result(&gZmodDev, gAdcResult);
  if (ret) {
    printf("ERROR: ZMOD4410 read ADC failed. ret=%d\n", ret);
    return -1;
  }

  // Check validity of the ADC results.
  ret = zmod4xxx_check_error_event(&gZmodDev);
  if (ret) {
    printf("ERROR: ZMOD4410 error event. ret=%d\n", ret);
    return -1;
  }

  // Assign algorithm inputs: raw sensor data and ambient conditions.
  gAlgoInput.adc_result = gAdcResult;
  if (isnan(aHumi)) {
    gAlgoInput.humidity_pct = 50.0;

  } else {
    gAlgoInput.humidity_pct = aHumi;
  }
  if (isnan(aTemp)) {
    gAlgoInput.temperature_degc = 20.0;

  } else {
    gAlgoInput.temperature_degc = aTemp;
  }

  // Calculate algorithm results
  ret = calc_iaq_2nd_gen_ulp(&gAlgoHandle, &gZmodDev, &gAlgoInput, &algo_results);

  // Check validity of the algorithm results.
  if (ret == IAQ_2ND_GEN_ULP_STABILIZATION) {
    DEBUG_PRINTLINE("ZMOD4410 warm up: etoh=%.3f ppm, tvoc=%.3f mg/m^3, eco2=%.0f ppm, iaq=%.1f", algo_results.etoh,
                    algo_results.tvoc, algo_results.eco2, algo_results.iaq);

    aResult->warmUp = true;
    aResult->eco2 = algo_results.eco2;
    aResult->etoh = algo_results.etoh;
    aResult->iaq = algo_results.iaq;
    aResult->tvoc = algo_results.tvoc;
  } else if (ret == IAQ_2ND_GEN_ULP_OK) {
    DEBUG_PRINTLINE("ZMOD4410 OK: etoh=%.3f ppm, tvoc=%.3f mg/m^3, eco2=%.0f ppm, iaq=%.1f", algo_results.etoh, algo_results.tvoc,
                    algo_results.eco2, algo_results.iaq);
    aResult->valid = true;
    aResult->eco2 = algo_results.eco2;
    aResult->etoh = algo_results.etoh;
    aResult->iaq = algo_results.iaq;
    aResult->tvoc = algo_results.tvoc;
  } else if (ret == IAQ_2ND_GEN_ULP_DAMAGE) {
    printf("ERROR: ZMOD4410 probably damaged. Algorithm results may be incorrect.\n");
    return -1;

  } else {
    printf("ERROR: ZMOD4410 got unexpected Error during algorithm calculation, ret=%d\n", ret);
    return -1;
  }

  return 0;
}

//==========================================================================
// Return the measurement time, in ms
//==========================================================================
uint32_t AirQualityMeasTime(void) { return ZMOD4410_IAQ2_ULP_SEQ_RUN_TIME_WITH_MARGIN; }

//==========================================================================
// Return the interval for each measurement, in ms
//==========================================================================
uint32_t AirQualityMeasInterval(void) { return ZMOD4410_IAQ2_ULP_SAMPLE_TIME; }