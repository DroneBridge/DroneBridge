//
// Created by cyber on 09.12.17.
//

#include "lib.h"
#include "db_protocol.h"

#ifndef CONTROL_STATUS_SHARED_MEMORY_H
#define CONTROL_STATUS_SHARED_MEMORY_H

wifibroadcast_rx_status_t *wbc_status_memory_open();
db_rc_values *db_rc_values_memory_open();
void db_rc_values_memory_init(db_rc_values *rc_values);

#endif //CONTROL_STATUS_SHARED_MEMORY_H
