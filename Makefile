CC = mpicc
RM = /bin/rm
PROG = mm mm_abft lu lu_abft kmeans kmeans_abft
CFLAGS = -lm -lz

DIR_SRC = ./src
DIR_BIN = ./bin
LIB_ABFT = ${DIR_SRC}/abft.c
DIR_MM = ${DIR_SRC}/mm
DIR_LU = ${DIR_SRC}/lu
DIR_KMEANS = ${DIR_SRC}/kmeans

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

lu_abft : ${DIR_BIN}/lu_abft
${DIR_BIN}/lu_abft : ${DIR_LU}/lu_abft.c ${LIB_ABFT}
	${CC} -o $@ $^ ${CFLAGS}

kmeans : ${DIR_BIN}/kmeans
${DIR_BIN}/kmeans : ${DIR_KMEANS}/kmeans.c 
	${CC} -o $@ $< ${CFLAGS}

kmeans_abft : ${DIR_BIN}/kmeans_abft
${DIR_BIN}/kmeans_abft : ${DIR_KMEANS}/kmeans_abft.c ${LIB_ABFT}
	${CC} -o $@ $^ ${CFLAGS}

clean :
	${RM} -f ${DIR_BIN}/*






