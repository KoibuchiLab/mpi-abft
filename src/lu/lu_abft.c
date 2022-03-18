// modified from https://github.com/platipo/MPI-LU-fact
// Author: huyao 220315

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <math.h>
#include <unistd.h>
#include "../../include/abft.h"

#define ln() putchar('\n')
// #define GENERIC_TAG (0)

double *gen_mx (size_t dim);
double *gen_row(size_t dim);
double *gen_row_ref (size_t dim, size_t ref);
void print_mx (double *M, size_t dim, size_t sep);
void forw_elim(double **origin, double *master_row, size_t dim);
void U_print (double *M, int dim);
void L_print (double *M, int dim);

int main(int argc, char *argv[])
{
   srand(time(NULL));
   const int root_p = 0;
   int mx_size = 0, p, id;
   if (argc < 2) {
      printf("Matrix size missing in the arguments\n");
      return EXIT_FAILURE;
   }
   mx_size = atol(argv[1]);
   double *A = gen_mx(mx_size);

   MPI_Init(NULL, NULL);
   MPI_Comm_size(MPI_COMM_WORLD, &p);
   MPI_Comm_rank(MPI_COMM_WORLD, &id);

   if (id == root_p) {
      printf("[A]\n");
      print_mx(A, mx_size * mx_size, mx_size);
   }   

   int i, j, tmp_size = mx_size - 1, diag_ref = 0;
   double start = MPI_Wtime();
 
   srand((unsigned)time(NULL));     
   for (i = 0; i < tmp_size; i++, diag_ref++) {
      double *diag_row = &A[diag_ref * mx_size + diag_ref];
      for (j = diag_ref + 1; j < mx_size; j++) {
         if (j % p == id) {
            double *save = &A[j * mx_size + diag_ref];
            //printf("[%d] ", id);
            //print_mx(save, mx_size - diag_ref, mx_size - diag_ref);
            forw_elim(&save, diag_row, mx_size - diag_ref);
         }
      }

      for (j = diag_ref + 1; j < mx_size; j++) {
         double *save = &A[j * mx_size + diag_ref];

         int size = mx_size - diag_ref;   
         int root = j % p;  
         
         MPI_Bcast_abft(save, size, root, id, p);
      }
   }

   double end = MPI_Wtime();

   if (id == root_p) {
      // printf("[LU]\n");
      // print_mx(A, mx_size * mx_size, mx_size);
      printf("\n[L]\n");
      L_print(A, mx_size);
      printf("\n[U]\n");
      U_print(A, mx_size);
      ln();
      // printf("mpi: %f s\n", end - start);
      // ln();
   }
   free(A);

   MPI_Finalize();
   return EXIT_SUCCESS;
}

/*
 * gen_mx - generate contiguous matrix
 *
 * @dim dim x dim matrix
 * @return matrix
 */
double *gen_mx (size_t dim)
{
   int i, j, tot = dim * dim;
   double *M = malloc(sizeof(double) * tot);
   for (i = 0; i < tot; i++) {
      M[i] = rand() % 101 - 50;
   }

   return M;
}

/*
 * mx_print - dumb matrix print function
 *
 * @M matrix/row
 * @dim matrix/row dimension
 * @sep where put separator
 */
void print_mx (double *M, size_t dim, size_t sep)
{
   int i, j;
   for (i = 0; i < dim; i++) {
      printf("% *.*f\t", 4, 2, M[i]);
      if ((i + 1) % sep == 0) {
         ln();
      }
   }
}

/*
 * forw_elim - forward Gauss elimination
 *
 * @origin row pointer by reference
 * @master_row row in which lays diagonal
 */
void forw_elim(double **origin, double *master_row, size_t dim)
{
   if (**origin == 0)
      return;

   double k = **origin / master_row[0];

   int i;
   for (i = 1; i < dim; i++) {
      (*origin)[i] = (*origin)[i] - k * master_row[i];
   }
   **origin = k;
}

/*
 * U_print - dumb U matrix print function
 */
void U_print (double *M, int dim)
{
   int i, j;
   double z = 0;
   for (i = 0; i < dim; i++) {
      for (j = 0; j < dim; j++) {
         if (j >= i) {
            printf("% *.*f\t", 4, 2, M[i * dim + j]);
         } else {
            printf("% *.*f\t", 4, 2, z);
         }
      }
      ln();
   }
}

/*
 * L_print - dumb L matrix print function
 */
void L_print (double *M, int dim)
{
   int i, j;
   double z = 0, u = 1;
   for (i = 0; i < dim; i++) {
      for (j = 0; j < dim; j++) {
         if (j > i) {
            printf("% *.*f\t", 4, 2, z);
         } else if (i == j) {
            printf("% *.*f\t", 4, 2, u);
         } else {
            printf("% *.*f\t", 4, 2, M[i * dim + j]);
         }
      }
      ln();
   }
}
