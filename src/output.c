#include <string.h>
#include "hmon/harray.h"
#include "hmon.h"
#include "./internal.h"

char * output_dir = NULL;
harray output_monitors = NULL;

struct output_data{
  harray monitors;
  FILE * output;
  char * id;
};

struct output_data * new_output_data(const char * id){
  struct output_data * d;
  size_t len = strlen("/dev/stdout")+1;
  if(output_dir != NULL) len = strlen(output_dir) + strlen(id) + 1;
  char output_path[len];
  if(output_dir == NULL){snprintf(output_path, len, "%s", "/dev/stdout");}
  else{sprintf(output_path, "%s%s", output_dir, id);}
  
  malloc_chk(d,sizeof(*d));
  d->id = strdup(id);
  d->output = fopen(output_path,"w");
  if(d->output == NULL){
    monitor_print_err("Cannot open %s for writing\n", output_path);
    perror_EXIT("fopen");
  }
  d->monitors = new_harray(sizeof(hmon), 64, NULL);
  return d;
}

void delete_output_data(struct output_data * d){
  if(d!= NULL){
    delete_harray(d->monitors);
    fclose(d->output);
    free(d->id);
  }
}

int output_data_compare(void * a, void * b)
{
  struct output_data * d_a = *(struct output_data**)a;
  struct output_data * d_b = *(struct output_data**)b;
  return strcmp(d_a->id,d_b->id);
}


void hmon_output_init(const char * dir){
  if(dir == NULL){output_dir = NULL;}
  else{output_dir = strdup(dir);}
  output_monitors = new_harray(sizeof(struct output_data *), 256, (void (*)(void*))delete_output_data);
}

void hmon_output_register_monitor(hmon m){
  struct output_data * d, key = {NULL, NULL, m->id};
  int i = harray_find(output_monitors, (void*)(&key), output_data_compare);
  if(i>=0){
    d = harray_get(output_monitors, i);
  }
  else{
    d = new_output_data(m->id);
    harray_insert_sorted(output_monitors, d, output_data_compare);
  }
  harray_push(d->monitors, m);
  m->output = d->output;
}

void hmon_output_monitors(){
  struct output_data * d;
  int i, j;
  for(i=0;i<harray_length(output_monitors); i++){
    d = harray_get(output_monitors,i);
    for(j=0;j<harray_length(d->monitors); j++){
      hmonitor_output(harray_get(d->monitors,j));
    }
  }
}

void hmon_output_fini(){
  delete_harray(output_monitors);
  free(output_dir);
}



