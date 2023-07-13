idf_component_register(SRCS "nmea_parser.c" "GPS_Distance.c" "ublox7.c"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS "include"
                       REQUIRES main)