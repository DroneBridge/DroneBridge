#pragma once

#include <stdint.h>
#include <time.h>
#include "../common/shared_memory.h"
#include "osdconfig.h"

typedef struct {
    uint32_t validmsgsrx;
    uint32_t datarx;

    float voltage;
    float ampere;
    int32_t mah;
    float baro_altitude;
    float altitude;
    double longitude;
    double latitude;
    float heading;
    float cog; //course over ground
    float speed;
    float airspeed;
    float roll, pitch;
    uint8_t sats;
    uint8_t fix;
    uint8_t armed;
    uint8_t rssi;

    uint8_t home_fix;

//#if defined(FRSKY)
    int16_t x, y, z; // also needed for smartport
    int16_t ew, ns;
//#endif

#if defined(SMARTPORT)
    uint8_t swr;
    float rx_batt;
    float adc1;
    float adc2;
    float vario;
#endif

#if defined(MAVLINK)
    uint32_t mav_flightmode;
    float mav_climb;
#endif

#if defined(LTM)
    // ltm S frame
    uint8_t ltm_status;
    uint8_t ltm_failsafe;
    uint8_t ltm_flightmode;
// ltm N frame
    uint8_t ltm_gpsmode;
    uint8_t ltm_navmode;
    uint8_t ltm_navaction;
    uint8_t ltm_wpnumber;
    uint8_t ltm_naverror;
// ltm X frame
    uint16_t ltm_hdop;
    uint8_t ltm_hw_status;
    uint8_t ltm_x_counter;
    uint8_t ltm_disarm_reason;
// ltm O frame
    float ltm_home_altitude;
    double ltm_home_longitude;
    double ltm_home_latitude;
    uint8_t ltm_osdon;
    uint8_t ltm_homefix;
#endif


    db_gnd_status_t *rx_status;
    db_rc_status_t *rx_status_rc;
    db_uav_status_t *rx_status_sysair;
} telemetry_data_t;

void telemetry_init(telemetry_data_t *td);


