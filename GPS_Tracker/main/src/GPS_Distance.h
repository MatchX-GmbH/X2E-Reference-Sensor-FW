//==========================================================================
//==========================================================================

#ifndef COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_
#define COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_

//==========================================================================
//==========================================================================
double UpdateRawDistance(double aLat, double aLon, double *aTotalDistance);
double UpdateAverageDistance(double aLat, double aLon, double *aTotalAverageDistance);

//==========================================================================
//==========================================================================
#endif /* COMPONENTS_UBLOX7C_INCLUDE_GPS_DISTANCE_H_ */
