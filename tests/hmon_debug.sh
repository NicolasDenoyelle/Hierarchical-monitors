#!/bin/bash

PDIR=../src/plugins
PLIST=("papi" "maqao" "learning" "fake" "accumulate" "hierarchical" "system" "stat_default")

echo "${#PLIST[@]} plugins:"

for i in $(seq 1 1 ${#PLIST[@]}); do
    printf "\t${PLIST[$i]}\n"
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PDIR/${PLIST[$i]}/.libs
done

if [ -z $1 ]; then
    OPTIONS=("-i test_monitor" "-f 100000" "-o ./output/")
else
    OPTIONS=$*
fi

echo "Options: "
for i in $(seq 1 1 ${#OPTIONS[@]}); do
    printf "\t${OPTIONS[$i]}\n"
done

#--eval-command "handle SIGINT pass
#--eval-command "b hmon_output_register_monitor"
libtool --mode=execute gdb --quiet --eval-command "handle SIG34 pass noprint nostop" --args ../src/hmonitor ${OPTIONS[*]}

