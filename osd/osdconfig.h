/* TELEMETRY PROTOCOL */
/* ------------------------------------------------------------*/
/* MAVLINK -> Mavlink protocol (APM/Pixhawk)
/* LTM -> Light Telemetry (iNav/Cleanflight/Betaflight)
/* FRSKY -> Older Frsky protocol (Frsky D-series receivers)
/* SMARTPORT -> Newer Frsky protocol (Frsky X-series receivers)
/* ------------------------------------------------------------*/
#define MAVLINK


/* MISC SETTINGS */
/* --------------*/
#define IMPERIAL false // set to true for imperial units, false for metric
#define COPTER true // set to true for copter, false for plane


/* DISPLAY */
/* --------*/
#define COLOR 255,255,255,0.3 // Fill color. First three numbers RGB color, last number opacity. 1.0 = fully visible, 0.0 = fully opaque
#define OUTLINECOLOR 0,0,0,0.7 // Outline color. First three numbers RGB color, last number opacity. 1.0 = fully visible, 0.0 = fully opaque
#define OUTLINEWIDTH 1 // Outline width, default is 1, useable range from 1-3. Set to 0 to disable outline
#define FONT "Archivo-Bold.ttf" // Font to use, case-sensitive!
#define GLOBAL_SCALE 1.2 // Global scale factor, 1.2 is default, useable range is from about 0.5 to 2


/* OSD elements positions, sizes and options */
/* ------------------------------------------*/
/* POS_X - from left to right (0-100)
/* POS_Y - from bottom to top (0-100)
/* SCALE - scale factor
/* ------------------------------------------*/
#define DOWNLINK_RSSI  // RSSI on GND of adapter with best reception
#define DOWNLINK_RSSI_POS_X 13
#define DOWNLINK_RSSI_POS_Y 89
#define DOWNLINK_RSSI_SCALE 1.2
#define DOWNLINK_RSSI_FEC_BAR false // set to true to draw FEC bar display

#define DOWNLINK_RSSI_DETAILED  // individual adapters in GND
#define DOWNLINK_RSSI_DETAILED_POS_X 9
#define DOWNLINK_RSSI_DETAILED_POS_Y 78
#define DOWNLINK_RSSI_DETAILED_SCALE 0.75

#define UPLINK_RSSI  // RC RSSI on UAV
#define UPLINK_RSSI_POS_X 92
#define UPLINK_RSSI_POS_Y 89
#define UPLINK_RSSI_SCALE 1.1

//#define RSSI
//#define RSSI_POS_X 32
//#define RSSI_POS_Y 89
//#define RSSI_SCALE 1

#define KBITRATE
#define KBITRATE_POS_X 73
#define KBITRATE_POS_Y 91
#define KBITRATE_SCALE 0.8

#define SYS
#define SYS_POS_X 91
#define SYS_POS_Y 78
#define SYS_SCALE 0.7

#define HOME_ARROW
#define HOME_ARROW_POS_X 50
#define HOME_ARROW_POS_Y 78
#define HOME_ARROW_SCALE 1
#define HOME_ARROW_USECOG false // use course over ground (from gps) instead of magnetometer
#define HOME_ARROW_INVERT false // set to true if arrow points in the opposite direction

#define BATT_MAH
#define BATT_MAH_POS_X 19
#define BATT_MAH_POS_Y 15
#define BATT_MAH_SCALE 1

#define BATT_STATUS
#define BATT_STATUS_POS_X 19
#define BATT_STATUS_POS_Y 6
#define BATT_STATUS_SCALE 1
#define BATT_STATUS_CURRENT true // set to true to draw current (ampere) (mavlink only)

#define BATT_GAUGE
#define BATT_GAUGE_POS_X 3
#define BATT_GAUGE_POS_Y 6
#define BATT_GAUGE_SCALE 1
#define CELLS 3 // set to number of cells used
#define CELL_MAX 4.20 // maximum cell voltage
#define CELL_MIN 3.20 // minimum cell voltage
#define CELL_WARNING1 3.50 // warning level 1 -> orange, low
#define CELL_WARNING2 3.40 // warning level 2 -> red, critical

#define COMPASS
#define COMPASS_POS_Y 87
#define COMPASS_SCALE 1
#define COMPASS_USECOG false // set to true to use course over ground (from gps) instead of magnetometer

#define ALTLADDER
#define ALTLADDER_POS_X 73
#define ALTLADDER_SCALE 1.2
#define ALTLADDER_USEBAROALT false // set to true to use barometer altitude instead of gps altitude

#define SPEEDLADDER
#define SPEEDLADDER_POS_X 27
#define SPEEDLADDER_SCALE 1.2
#define SPEEDLADDER_USEAIRSPEED false // set to true to use GPS speed instead of airspeed

#define AHI
#define AHI_SCALE 1.2
#define AHI_LADDER false // set to true to draw ladders above and below the center horizon line
#define AHI_INVERT_ROLL -1 // default -1, set to 1 if roll moves in the wrong direction
#define AHI_INVERT_PITCH 1 // default -1, set to 1 if pitch moves in the wrong direction
#define AHI_SWAP_ROLL_AND_PITCH true // set to true to swap roll and pitch (Frsky and Smartport only)

//#define POSITION
//#define POSITION_POS_X 76
//#define POSITION_POS_Y 6
//#define POSITION_SCALE 0.8

#define SAT
#define SAT_POS_X 6
#define SAT_POS_Y 12
#define SAT_SCALE 0.8

#define DISTANCE
#define DISTANCE_POS_X 95
#define DISTANCE_POS_Y 6
#define DISTANCE_SCALE 1

#define FLIGHTMODE
#define FLIGHTMODE_POS_X 42
#define FLIGHTMODE_POS_Y 6
#define FLIGHTMODE_SCALE 1

#define CLIMB
#define CLIMB_POS_X 93
#define CLIMB_POS_Y 26
#define CLIMB_SCALE 0.8

#define AIRSPEED
#define AIRSPEED_POS_X 93
#define AIRSPEED_POS_Y 21
#define AIRSPEED_SCALE 0.8

#define BAROALT
#define BAROALT_POS_X 93
#define BAROALT_POS_Y 16
#define BAROALT_SCALE 0.8

//#define COURSE_OVER_GROUND
#define COURSE_OVER_GROUND_POS_X 92
#define COURSE_OVER_GROUND_POS_Y 65
#define COURSE_OVER_GROUND_SCALE 0.8

//#define GPSSPEED
#define GPSSPEED_POS_X 93
#define GPSSPEED_POS_Y 60
#define GPSSPEED_SCALE 0.8

//#define GPSALT
#define GPSALT_POS_X 92
#define GPSALT_POS_Y 55
#define GPSALT_SCALE 0.8

#define WARNING_POS_X 50
#define WARNING_POS_Y 25

