#!/bin/sh -x
if [ $# -ne 2 ]; then
    echo "This script cannot receive $# input(s)."
    echo "Usage: $0 #of processes and #of threads "
    exit 1
fi

NP=$1
NT=$2

Host=`hostname -s`
Date=`date +%y%m%d-%H%M%S`
LogDir=logs
mkdir -p ${LogDir}
LogFile=${LogDir}/run-${NP}p-${Host}-${Date}

mpirun\
    -n $NP \
    -npernode 1 \
    -hostfile hostfile_8nodes.txt \
    -mca btl_openib_allow_ib true -mca btl openib,self \
    -x OMP_NUM_THREADS=$NT \
    -x PATH -x LD_LIBRARY_PATH \
    taskset -c 0-9 ./xhpl 2>&1 | tee -a ${LogFile}
