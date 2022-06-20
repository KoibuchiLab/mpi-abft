#!/bin/sh

PATH=/home/proj/atnw/nagasaka/gcc/8.5.0/bin:${PATH}
LD_LIBRARY_PATH=/home/proj/atnw/nagasaka/mpc/1.1.0/lib:${LD_LIBRARY_PATH}
LD_LIBRARY_PATH=/home/proj/atnw/nagasaka/gcc/9.4.0/lib64:${LD_LIBRARY_PATH}
source /home/proj/atnw/honda/intel/oneapi/setvars.sh

PATH=/home/proj/atnw/local/bin:${PATH}
LD_LIBRARY_PATH=/home/proj/atnw/local/lib:${LD_LIBRARY_PATH}

export LC_ALL=C
export PATH
export LD_LIBRARY_PATH
