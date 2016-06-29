#include <hmon.h>

int main(){
    monitor_lib_init(NULL, NULL, "/dev/stdout");
    monitors_import("./example_monitor");
    monitors_start();
    monitors_update();
    sleep(1);
    monitors_update();
    monitors_output(monitor_output, 1);
    monitor_lib_finalize();
    return 0;
}

