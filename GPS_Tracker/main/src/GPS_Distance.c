//==========================================================================
// Application
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

#include "GPS_Distance.h"
#include <math.h>

#define COORD_AVG_SIZE 5

#define TAG "GPS_Distance.c"

typedef struct {
  double Lat;
  double Lon;
} coordinate_t;

coordinate_t coord_List[COORD_AVG_SIZE];
uint8_t coord_Pointer = 0;
double prev_lat = 0, prev_lon = 0;
double prev_Average_lat = 0, prev_Average_lon = 0;
double TotalAverageDistance;
double TotalDistance;

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

double UpdateRawDistance(double lat, double lon) {
  if ((prev_lat != 0) && (prev_lon != 0)) {
    TotalDistance += Spherical_Law_of_Cosines(prev_lat, prev_lon, lat, lon);
  }

  prev_lat = lat;
  prev_lon = lon;

  return TotalDistance;
}

double UpdateAverageDistance(double lat, double lon) {

  coordinate_t coord = {
      .Lat = lat,
      .Lon = lon };

  coord_List[coord_Pointer] = coord;
  coord_Pointer++;

  if (coord_Pointer == COORD_AVG_SIZE) {

    coordinate_t avg_Coord = {
        .Lat = 0,
        .Lon = 0 };
    coord_Pointer = 0;

    for (uint8_t i = 0; i < COORD_AVG_SIZE; i++) {
      avg_Coord.Lat += coord_List[i].Lat;
      avg_Coord.Lon += coord_List[i].Lon;
    }
    avg_Coord.Lat /= (double) COORD_AVG_SIZE;
    avg_Coord.Lon /= (double) COORD_AVG_SIZE;

    if ((prev_Average_lat != 0) && (prev_Average_lon != 0)) {
      TotalAverageDistance += Spherical_Law_of_Cosines(prev_Average_lat, prev_Average_lon, avg_Coord.Lat, avg_Coord.Lon);
    }
    prev_Average_lat = avg_Coord.Lat;
    prev_Average_lon = avg_Coord.Lon;
  }

  return TotalAverageDistance;
}

