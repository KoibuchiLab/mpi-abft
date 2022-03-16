CC = mpicc
RM = /bin/rm
PROG = mm mm_abft lu
CFLAGS = -lm -lz

DIR_SRC = ./src
DIR_BIN = ./bin
LIB_ABFT = ${DIR_SRC}/abft.c
DIR_MM = ${DIR_SRC}/mm
DIR_LU = ${DIR_SRC}/lu

all : ${PROG}

mm : ${DIR_BIN}/mm
${DIR_BIN}/mm : ${DIR_MM}/mm.c 
	${CC} -o $@ $< ${CFLAGS}

mm_abft : ${DIR_BIN}/mm_abft
${DIR_BIN}/mm_abft : ${DIR_MM}/mm_abft.c ${LIB_ABFT}
	${CC} -o $@ $^ ${CFLAGS}

lu : ${DIR_BIN}/lu
${DIR_BIN}/lu : ${DIR_LU}/lu.c 
	${CC} -o $@ $< ${CFLAGS}

clean :
	${RM} -f ${DIR_BIN}/*






