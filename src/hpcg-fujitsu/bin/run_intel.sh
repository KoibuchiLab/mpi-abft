#!/bin/bash

SCRIPT=/home/proj/atnw/nagasaka/script
HPCG_HOME=/home/proj/atnw/nagasaka/hpcg
NPROCS=20
HOSTFILE=${HPCG_HOME}/hostfile

source ${SCRIPT}/env_intel.sh

cd ${HPCG_HOME}/bin
OMP_NUM_THREADS=1 mpirun -f ${HOSTFILE} -np ${NPROCS} -ppn 20 ./xhpcg_skx 64 64 64 30
