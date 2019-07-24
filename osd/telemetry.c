#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <stdlib.h>
#include "telemetry.h"
#include "osdconfig.h"
#include "../common/shared_memory.h"

void telemetry_init(telemetry_data_t *td) {
	td->validmsgsrx = 0;
	td->datarx = 0;

	td->voltage = 0;
	td->ampere = 0;
	td->mah = 0;
	td->baro_altitude = 0;
	td->altitude = 0;
	td->longitude = 0;
	td->latitude = 0;
	td->heading = 0;
	td->cog = 0;
	td->speed = 0;
	td->airspeed = 0;
	td->roll = 0;
	td->pitch = 0;
	td->sats = 0;
	td->fix = 0;
	td->armed = 255;
	td->rssi = 0;
	td->home_fix = 0;

#ifdef FRSKY
	td->x = 0;
	td->y = 0;
	td->z = 0;
	td->ew = 0;
	td->ns = 0;
#endif

#ifdef MAVLINK
	td->mav_flightmode = 255;
    td->mav_climb = 0;
#endif

#ifdef LTM
	// ltm S frame
    td->ltm_status = 0;
    td->ltm_failsafe = 0;
    td->ltm_flightmode = 0;
// ltm N frame
    td->ltm_gpsmode = 0;
    td->ltm_navmode = 0;
    td->ltm_navaction = 0;
    td->ltm_wpnumber = 0;
    td->ltm_naverror = 0;
// ltm X frame
    td->ltm_hdop = 0;
    td->ltm_hw_status = 0;
    td->ltm_x_counter = 0;
    td->ltm_disarm_reason = 0;
// ltm O frame
    td->ltm_home_altitude = 0;
    td->ltm_home_longitude = 0;
    td->ltm_home_latitude = 0;
#endif

    td->rx_status = db_gnd_status_memory_open();

#ifdef UPLINK_RSSI
	td->rx_status_rc = db_rc_status_memory_open();
#endif

	td->rx_status_sysair = db_uav_status_memory_open();
}
