#!/bin/sh

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/plugins/papi:$PWD/plugins/maqao:$PWD/plugins/learning:$PWD/plugins/stat_default:$PWD/plugins/fake:$PWD/plugins/accumulate:$PWD/plugins/hierarchical

libtool --mode=execute gdb --eval-command "handle SIG34 pass noprint nostop" --eval-command "handle SIGINT pass" --args ./hmonitor --input ../example/example_monitor -f 100000 -d -o /dev/null

