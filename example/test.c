#include <hmon.h>
#include <stdio.h>

void usage(const char * argv0){
  printf("%s <input_monitors>\n", argv0);
}

int main(int argc, char ** argv){
  if(argc!=2){usage(argv[0]); return -1;}
  
  hmon_lib_init(NULL, "/dev/stdout");
  hmon_import_hmonitors(argv[1]);
  hmon_start();
  hmon_update();
  sleep(1);
  hmon_update();
  hmon_lib_finalize();
  return 0;
}

