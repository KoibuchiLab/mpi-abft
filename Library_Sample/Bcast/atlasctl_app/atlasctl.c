/*
	    Copyright (c) NEC Corporation 2017. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define debug_printf printf 
#define NODE_MAX 8

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <mpi.h>
#include <sys/time.h>
#include "Lib_xbar/Xbar_SR.h"

int rank,root,size;

int main(int argc, char *argv[])
{
	unsigned int ii,jj;
	unsigned int *input_data=NULL;
	unsigned int *output_data=NULL;
	unsigned int data_num = 0;;
	int send_rank=0;
	

	OPTWEB_MPI_Init(&argc, &argv);
	OPTWEB_MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size); // "size" is the number of processes

	/* main process */
	data_num=atoi(argv[1])/sizeof(int);

	input_data = (int*)malloc(sizeof(int)*data_num*NODE_MAX);
	output_data = (int*)malloc(sizeof(int)*data_num*NODE_MAX);
	
	for(jj=0; jj<NODE_MAX; jj++)
		for(ii=0; ii<data_num; ii++)
			input_data[jj*data_num+ii] =ii;

	MPI_Barrier( MPI_COMM_WORLD );
	printf("Start Bcast Size:%dByte rank:%d\n", atoi(argv[1]), rank);
	if(rank==send_rank)
		OPTWEB_MPI_Bcast(input_data, data_num, MPI_INT, send_rank, MPI_COMM_WORLD);
	else
		OPTWEB_MPI_Bcast(output_data, data_num, MPI_INT, send_rank, MPI_COMM_WORLD);
	printf("End Bcast rank:%d\n", rank);

	/* close devices */
	OPTWEB_MPI_Finalize();
	if(input_data)
		free(input_data);
	if(output_data)
		free(output_data);
	return 0;
}

