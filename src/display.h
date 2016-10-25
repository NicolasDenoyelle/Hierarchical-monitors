#ifndef DISPLAY_H
#define DISPLAY_H

#include <hwloc.h>

void hmon_display_init(hwloc_topology_t topology);
void hmon_display_finalize();
void hmon_display_refresh(int verbose);

#endif
