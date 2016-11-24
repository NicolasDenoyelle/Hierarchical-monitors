#include <private/autogen/config.h>
#include <hmon.h>
#include <hwloc/lstopo.h>
#include <unistd.h>

static hwloc_topology_t display_topology = NULL;
static struct lstopo_output display_output;
int is_initialized = 0;

static int callback(struct lstopo_output *loutput, hwloc_obj_t obj, unsigned depth, unsigned x, unsigned width, unsigned y, unsigned height)
{
  struct draw_methods *methods = loutput->methods;
  unsigned fontsize = loutput->fontsize;
  unsigned gridsize = loutput->gridsize;
  harray _monitors = hmon_get_monitors_by_depth(obj->depth, obj->logical_index);
  if(_monitors == NULL){return -1;}
  hmon monitor;
  unsigned i;

  /* find a printable monitor */
  for(i=0; i<harray_length(_monitors); i++){
    monitor = harray_get(_monitors,i);
    if(monitor != NULL && monitor->display && monitor->display <= (int)monitor->n_samples)
      break;
  }
  if(monitor == NULL || monitor->display == 0 || monitor->display > (int)monitor->n_samples){return -1;}
  
  double max = monitor->max[monitor->display-1];
  double min = monitor->min[monitor->display-1];
  double val = (monitor->samples[monitor->display-1] - min)/(max-min);
  char value[16]; memset(value, 0, sizeof(value));
  int r = val>0.5 ? 255         : 510*val;
  int g = val>0.5 ? 510*(1-val) : 255;
  int b = 0;  

  switch (obj->type) {
  case HWLOC_OBJ_MACHINE:
  case HWLOC_OBJ_PACKAGE:
    snprintf(value, sizeof(value), "%-.6e", monitor->samples[monitor->display-1]);
    methods->box(loutput, r, g, b, depth, x, width, y, height);
    methods->text(loutput, 0, 0, 0, fontsize, depth, x+gridsize, y+gridsize, value);
    return 0;
  case HWLOC_OBJ_CORE:
    snprintf(value, sizeof(value), "%-.2e", monitor->samples[monitor->display-1]);
    methods->box(loutput, r, g, b, depth, x, width, y, height);
    methods->text(loutput, 0, 0, 0, fontsize, depth, x+gridsize, y+gridsize, value);
    return 0;
  case HWLOC_OBJ_PU:
    snprintf(value, sizeof(value), "%-.1e", monitor->samples[monitor->display-1]);
    methods->box(loutput, r, g, b, depth, x, width, y, height);
    methods->text(loutput, 0, 0, 0, fontsize, depth, x+gridsize, y+gridsize, value);
    return 0;
  case HWLOC_OBJ_NUMANODE:
    snprintf(value, sizeof(value), "%-.6e", monitor->samples[monitor->display-1]);
    methods->box(loutput, 0xd2, 0xe7, 0xa4, depth, x, width, y, height);
    methods->box(loutput, r, g, b, depth, x+gridsize, width-2*gridsize, y+gridsize, fontsize+2*gridsize);
    methods->text(loutput, 0, 0, 0, fontsize, depth, x+2*gridsize, y+2*gridsize, value);
    return 0;
  case HWLOC_OBJ_L1CACHE:
  case HWLOC_OBJ_L2CACHE:
  case HWLOC_OBJ_L3CACHE:
  case HWLOC_OBJ_L4CACHE:
  case HWLOC_OBJ_L5CACHE:
  case HWLOC_OBJ_L1ICACHE:
  case HWLOC_OBJ_L2ICACHE:
  case HWLOC_OBJ_L3ICACHE:
    snprintf(value, sizeof(value), "%-.2e", monitor->samples[monitor->display-1]);
    methods->box(loutput, r, g, b, depth, x, width, y, height);
    methods->text(loutput, 0, 0, 0, fontsize, depth, x+gridsize, y+gridsize, value);
    return 0;
  default:
    return -1;
  }
}


void hmon_display_init(hwloc_topology_t topology){
  hwloc_topology_dup(&display_topology, topology);
  if(display_topology == NULL) return;
  
  lstopo_init(&display_output);
  display_output.logical = 0;
  display_output.topology = display_topology;
  display_output.drawing_callback = callback;
  lstopo_prepare(&display_output);
  is_initialized = 1;
  output_x11(&display_output, NULL);
}

void hmon_display_finalize(){
  if(!is_initialized){
    fprintf(stderr, "Request to lstopo display finalize but is not initialized.\n");
    return;
  }
  if(display_output.methods && display_output.methods->end) display_output.methods->end(&display_output);
  lstopo_destroy(&display_output);
  is_initialized = 0;
  hwloc_topology_destroy(display_topology);
}

int hmon_display_refresh(int verbose){
  if(is_initialized && display_output.methods && display_output.methods->iloop){
    if(display_output.methods->iloop(&display_output, 0) == -1){
      return -1;
    }
  }
  return 0;
}

