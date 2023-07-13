/*
 * GPS_Distance.h
 *
 *  Created on: 16 Jun 2023
 *      Author: moham_em3gyci
 */

#ifndef COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_
#define COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_
#include "nmea_parser.h"

struct GPSDataT {
  gps_t gps_data;
  double Total_Distance;
};

typedef struct GPSDataT GPSData_t;

double Update_Distance(double lat, double lon);
void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
QueueHandle_t GPS_Distane_INIT(bool *Distance_Lock);

#endif /* COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_ */
