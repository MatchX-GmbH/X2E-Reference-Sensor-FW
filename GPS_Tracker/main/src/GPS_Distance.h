/*
 * GPS_Distance.h
 *
 *  Created on: 16 Jun 2023
 *      Author: moham_em3gyci
 */

#ifndef COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_
#define COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_

double UpdateRawDistance(double lat, double lon);
double UpdateAverageDistance(double lat, double lon);

#endif /* COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_ */
