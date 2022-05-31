// modified from https://github.com/platipo/MPI-LU-fact
// Author: huyao 220531

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include "abft_optweb.h" // relative to this file
#include "Lib_xbar/Xbar_SR.h"

#define ln() putchar('\n')

unsigned int *gen_mx (size_t dim);
unsigned int *gen_row(size_t dim);
unsigned int *gen_row_ref (size_t dim, size_t ref);
void print_mx (unsigned int *M, size_t dim, size_t sep);
void forw_elim(unsigned int **origin, unsigned int *master_row, size_t dim);
void U_print (unsigned int *M, int dim);
void L_print (unsigned int *M, int dim);

int main(int argc, char *argv[])
{
   srand(time(NULL));
   // const int root_p = 0;
   int mx_size = 0, p, id;
   if (argc < 2) {
      printf("Matrix size missing in the arguments\n");
      return EXIT_FAILURE;
   }
   mx_size = atol(argv[1]);
   unsigned int *A = gen_mx(mx_size);

   OPTWEB_MPI_Init(NULL, NULL);
   MPI_Comm_size(MPI_COMM_WORLD, &p);
   OPTWEB_MPI_Comm_rank(MPI_COMM_WORLD, &id);

   // if (id == root_p) {
   //    printf("[A]\n");
   //    print_mx(A, mx_size * mx_size, mx_size);
   // }   

   int i, j, tmp_size = mx_size - 1, diag_ref = 0;
   // double start = MPI_Wtime();
 
   srand((unsigned)time(NULL));     
   for (i = 0; i < tmp_size; i++, diag_ref++) {
      unsigned int *diag_row = &A[diag_ref * mx_size + diag_ref];
      for (j = diag_ref + 1; j < mx_size; j++) {
         if (j % p == id) {
            unsigned int *save = &A[j * mx_size + diag_ref];
            //printf("[%d] ", id);
            //print_mx(save, mx_size - diag_ref, mx_size - diag_ref);
            forw_elim(&save, diag_row, mx_size - diag_ref);
         }
      }

      for (j = diag_ref + 1; j < mx_size; j++) {
         unsigned int *save = &A[j * mx_size + diag_ref];

         int size = mx_size - diag_ref;   
         int root = j % p;  
         
         // OPTWEB_MPI_Bcast(save, size, MPI_INT, root, MPI_COMM_WORLD);      
         MPI_Bcast_abft_optweb(save, size, root, id, p, 1); 
      }
   }

   // double end = MPI_Wtime();

   // if (id == root_p) {
   //    // printf("[LU]\n");
   //    // print_mx(A, mx_size * mx_size, mx_size);
   //    printf("\n[L]\n");
   //    L_print(A, mx_size);
   //    printf("\n[U]\n");
   //    U_print(A, mx_size);
   //    ln();
   //    // printf("mpi: %f s\n", end - start);
   //    // ln();
   // }
   free(A);

   OPTWEB_MPI_Finalize();
   return EXIT_SUCCESS;
}

/*
 * gen_mx - generate contiguous matrix
 *
 * @dim dim x dim matrix
 * @return matrix
 */
unsigned int *gen_mx (size_t dim)
{
   int i, tot = dim * dim;
   unsigned int *M = malloc(sizeof(int) * tot);
   for (i = 0; i < tot; i++) {
      M[i] = rand() % 1000 + 1;
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
void print_mx (unsigned int *M, size_t dim, size_t sep)
{
   unsigned int i;
   for (i = 0; i < dim; i++) {
      printf("%d\t", M[i]);
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
void forw_elim(unsigned int **origin, unsigned int *master_row, size_t dim)
{
   if (**origin == 0)
      return;

   unsigned int k = **origin / master_row[0];

   unsigned int i;
   for (i = 1; i < dim; i++) {
      (*origin)[i] = (*origin)[i] - k * master_row[i];
   }
   **origin = k;
}

/*
 * U_print - dumb U matrix print function
 */
void U_print (unsigned int *M, int dim)
{
   int i, j;
   unsigned int z = 0;
   for (i = 0; i < dim; i++) {
      for (j = 0; j < dim; j++) {
         if (j >= i) {
            printf("%u\t", M[i * dim + j]);
         } else {
            printf("%u\t", z);
         }
      }
      ln();
   }
}

/*
 * L_print - dumb L matrix print function
 */
void L_print (unsigned int *M, int dim)
{
   int i, j;
   unsigned int z = 0, u = 1;
   for (i = 0; i < dim; i++) {
      for (j = 0; j < dim; j++) {
         if (j > i) {
            printf("%u\t", z);
         } else if (i == j) {
            printf("%u\t", u);
         } else {
            printf("%u\t", M[i * dim + j]);
         }
      }
      ln();
   }
}
