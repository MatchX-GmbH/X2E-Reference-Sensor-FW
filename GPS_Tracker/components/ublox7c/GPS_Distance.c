/*
 * GPS_Distance.c
 *
 *  Created on: 16 Jun 2023
 *      Author: moham_em3gyci
 */

#include <math.h>
#include "GPS_Distance.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define dilution_of_precision 10
#define USR_LED (GPIO_NUM_21)
#define GPS_QUEUE_SIZE 500
#define GPS_INTERVAL 12

double prev_lat = 0, prev_lon = 0;

GPSData_t GPSData;

QueueHandle_t GPS_queue;
bool *Distance_Enable;
uint8_t GPSDataCount = 0;

//double Lat_Buf[AVG_filter_size], Lon_Buf[AVG_filter_size];

/**
 * @brief converts decimal degrees to radians
 *
 * @param deg Decimal degrees
 * @return double of radians value
 */
static inline double deg2rad(double deg) {
  return (deg * M_PI / 180.0);
}

/**
 * @brief converts radians to decimal degree
 *
 * @param rad radians value
 * @return double of Decimal degrees
 */
static inline double rad2deg(double rad) {
  return (rad * 180.0 / M_PI);
}

/**
 * @brief
 *
 * @param
 * @return double
 */
static double Spherical_Law_of_Cosines(double lat1, double lon1, double lat2, double lon2) {
  double theta, dist;
  if ((lat1 == lat2) && (lon1 == lon2)) {
    return 0;
  } else {
    theta = lon1 - lon2;
    dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
    dist = acos(dist);
    dist = rad2deg(dist);
    dist = dist * 60 * 1.1515;
    dist = dist * 1.609344;
    return (dist);
  }
}

double Update_Distance_RAW(double lat, double lon) {
  if ((prev_lat != 0) && (prev_lon != 0)) {
    GPSData.Total_Distance += Spherical_Law_of_Cosines(prev_lat, prev_lon, lat, lon);
  }

  prev_lat = lat;
  prev_lon = lon;

  return GPSData.Total_Distance;
}

void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  gps_t *gps = NULL;
  switch (event_id) {
    case GPS_UPDATE:
      gps = (gps_t*) event_data;

      GPSData.gps_data = *gps;
      bool Start_Measure = *Distance_Enable;

//      printf("dop_h = %0.3f, dop_v = %0.3f, dop_p = %0.3f\n", gps->dop_h, gps->dop_v, gps->dop_p);

      if ((gps->fix != GPS_FIX_INVALID) && (gps->dop_h <= dilution_of_precision) && (gps->dop_v <= dilution_of_precision)
          && (gps->dop_p <= dilution_of_precision)) {

//				printf("Lat = %0.3f, Lon = %0.3f, Alt = %0.3f\n", gps->latitude, gps->longitude, gps->altitude);

        if (((gps->speed > 0.7) && Start_Measure) || (gps->speed > 4.4)) {      // Check if not Idle
          Update_Distance_RAW(gps->latitude, gps->longitude);
          gpio_set_level(USR_LED, 0);
        } else
          gpio_set_level(USR_LED, 1);
//					printf("Total Distance = %0.3f,   Speed = %0.3f\n",Update_Distance_RAW(gps->latitude, gps->longitude),gps->speed);
        if (GPSDataCount >= GPS_INTERVAL) {
          xQueueSend(GPS_queue, (void* )&GPSData, (TickType_t )0);
          GPSDataCount = 0;
        } else
          GPSDataCount++;

      }

      break;
    default:
      break;
  }
}

QueueHandle_t GPS_Distane_INIT(bool *Distance_Lock) {

  Distance_Enable = Distance_Lock;

  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config, UB7_OUTPUT_1HZ);
  /* register event handler for NMEA parser library */
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);

  GPS_queue = xQueueCreate(GPS_QUEUE_SIZE, sizeof(GPSData));

  return GPS_queue;
}
