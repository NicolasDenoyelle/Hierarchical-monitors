#include <hmon.h>
#include <stdio.h>
#include <time.h>

void usage(const char * argv0){
  printf("%s <input_monitors>\n", argv0);
}

int main(int argc, char ** argv){
  if(argc!=2){usage(argv[0]); return -1;}
  
  hmon_lib_init(NULL, NULL);
  hmon_import_hmonitors(argv[1]);
  hmon_start();

  /* hmon_update(); */
  /* sleep(1); */
  /* hmon_update(); */
  
  if(hmon_sampling_start(10000) == -1){ fprintf(stderr, "error sampling start\n"); }
  system("sleep 1");
  if(hmon_sampling_stop() == -1){ fprintf(stderr, "error sampling stop\n"); }
  
  hmon_lib_finalize();
  return 0;
}

