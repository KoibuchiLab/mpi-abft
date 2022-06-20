#!/bin/bash

SCRIPT=/home/proj/atnw/nagasaka/script
HPCG_HOME=/home/proj/atnw/nagasaka/hpcg
NPROCS=2
HOSTFILE=${HPCG_HOME}/hostfile_ompi

source ${SCRIPT}/env_openmpi.sh

cd ${HPCG_HOME}/bin
export OMP_NUM_THREADS=1
mpirun -np ${NPROCS} -N 1 -hostfile ${HOSTFILE} --mca btl_openib_allow_ib 1 ./xhpcg_ompi

