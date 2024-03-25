//==========================================================================
// Buffer packing utils
//==========================================================================
#ifndef INC_MATCHX_PAYLOAD_H
#define INC_MATCHX_PAYLOAD_H
//==========================================================================
//==========================================================================
// Data type (D7..D4)
#define MX_DATATYPE_PARAM 0x00
#define MX_DATATYPE_SENSOR 0x10
#define MX_DATATYPE_BATTLEVEL 0x20
#define MX_DATATYPE_BATTPERCENT 0x30
#define MX_DATATYPE_EVENT 0x40

// Sensor data
#define MX_SENSOR_UNKNONW 0x00
#define MX_SENSOR_GPS 0x01
#define MX_SENSOR_TEMPERATURE 0x02
#define MX_SENSOR_HUMIDITY 0x03
#define MX_SENSOR_PRESSURE 0x04
#define MX_SENSOR_PM10 0x05
#define MX_SENSOR_PM2_5 0x06
#define MX_SENSOR_TVOC 0x07
#define MX_SENSOR_NO2 0x08
#define MX_SENSOR_CO2 0x09
#define MX_SENSOR_AIRFLOW 0x0a
#define MX_SENSOR_VOLTAGE 0x0b
#define MX_SENSOR_CURRENT 0x0c
#define MX_SENSOR_POWER 0x0d
#define MX_SENSOR_POWERUSAGE 0x0e
#define MX_SENSOR_WATERUSAGE 0x0f
#define MX_SENSOR_SPEED 0x10
#define MX_SENSOR_ROTATION 0x11
#define MX_SENSOR_COUNTER32 0x12
#define MX_SENSOR_DIGITAL 0x13
#define MX_SENSOR_PERCENT 0x14
#define MX_SENSOR_POWERFACTGOR 0x15
#define MX_SENSOR_ACCEL 0x16
#define MX_SENSOR_DISTANCE 0x17
#define MX_SENSOR_IAQ 0x18
#define MX_SENSOR_UPLINKPOWER 0xfe

//==========================================================================
//==========================================================================
#endif