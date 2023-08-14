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

//==========================================================================
// Defines
//==========================================================================
#define COORD_AVG_SIZE 5
#define TAG "GPS_Distance.c"

//==========================================================================
// Variables
//==========================================================================
typedef struct {
  double Lat;
  double Lon;
} coordinate_t;

coordinate_t gCoordList[COORD_AVG_SIZE];
uint8_t gCoordPointer = 0;
double gPrevLat = 0, gPrevLon = 0;
double gPrevAverageLat = 0, gPrevAverageLon = 0;
double gTotalAverageDistance;
double gTotalDistance;

/**
 * @brief converts decimal degrees to radians
 *
 * @param deg Decimal degrees
 * @return double of radians value
 */
static inline double deg2rad(double aDeg) {
  return (aDeg * M_PI / 180.0);
}

/**
 * @brief converts radians to decimal degree
 *
 * @param rad radians value
 * @return double of Decimal degrees
 */
static inline double rad2deg(double aRad) {
  return (aRad * 180.0 / M_PI);
}

/**
 * @brief
 *
 * @param
 * @return double
 */
static double Spherical_Law_of_Cosines(double aLat1, double aLon1, double aLat2, double aLon2) {
  double theta, dist;
  if ((aLat1 == aLat2) && (aLon1 == aLon2)) {
    return 0;
  } else {
    theta = aLon1 - aLon2;
    dist = sin(deg2rad(aLat1)) * sin(deg2rad(aLat2)) + cos(deg2rad(aLat1)) * cos(deg2rad(aLat2)) * cos(deg2rad(theta));
    dist = acos(dist);
    dist = rad2deg(dist);
    dist = dist * 60 * 1.1515;
    dist = dist * 1.609344;
    return (dist);
  }
}

double UpdateRawDistance(double aLat, double aLon) {
  if ((gPrevLat != 0) && (gPrevLon != 0)) {
    gTotalDistance += Spherical_Law_of_Cosines(gPrevLat, gPrevLon, aLat, aLon);
  }

  gPrevLat = aLat;
  gPrevLon = aLon;

  return gTotalDistance;
}

double UpdateAverageDistance(double aLat, double aLon) {

  coordinate_t coord = {
      .Lat = aLat,
      .Lon = aLon };

  gCoordList[gCoordPointer] = coord;
  gCoordPointer++;

  if (gCoordPointer == COORD_AVG_SIZE) {

    coordinate_t avg_Coord = {
        .Lat = 0,
        .Lon = 0 };
    gCoordPointer = 0;

    for (uint8_t i = 0; i < COORD_AVG_SIZE; i++) {
      avg_Coord.Lat += gCoordList[i].Lat;
      avg_Coord.Lon += gCoordList[i].Lon;
    }
    avg_Coord.Lat /= (double) COORD_AVG_SIZE;
    avg_Coord.Lon /= (double) COORD_AVG_SIZE;

    if ((gPrevAverageLat != 0) && (gPrevAverageLon != 0)) {
      gTotalAverageDistance += Spherical_Law_of_Cosines(gPrevAverageLat, gPrevAverageLon, avg_Coord.Lat, avg_Coord.Lon);
    }
    gPrevAverageLat = avg_Coord.Lat;
    gPrevAverageLon = avg_Coord.Lon;
  }

  return gTotalAverageDistance;
}

