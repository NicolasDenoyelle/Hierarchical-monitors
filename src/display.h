#ifndef DISPLAY_H
#define DISPLAY_H

#include <hwloc.h>

/**
 * Initialize display using \p topology pointer informations provided once. 
 * The \p topoology pointer should be stored internally in read only mode or duplicated.
 * @param topology, the topology to display.
 **/
void hmon_display_init(hwloc_topology_t topology);

/**
 * Destroy display internal storage if any.
 **/
void hmon_display_finalize();

/**
 * Update topology display with updated monitors.
 * @param verbose, will output more information with certain implementations.
 * @return 0 if update went fine, -1 if display do not respond anymore.
 **/
int hmon_display_refresh(int verbose);

#endif
