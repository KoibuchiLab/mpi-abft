# mpi-abft-apps
This repo contains several algorithm based fault tolerant apps.
Each app contains an original version and a modified abft version (_abft).

- CRC32 is used to verify the integrity of communication data.
- Hamming code (SECDED) is used to correct one-bit errors and detect two-bit errors in data blocks. 

## MM
- Compile
```
$ make mm
$ make mm_abft
```
- Run
```
$ mpirun -np <num_of_procs> bin/mm <matrix_a_filename> <matrix_b_filename>
$ mpirun -np <num_of_procs> bin/mm_abft <matrix_a_filename> <matrix_b_filename>
```
- A small program [gen_matrices.py](src/mm/gen_matrices.py) helps to generate a matrix. 
- Requirement
    - m_1->cols == m_2->rows
    - m_1->rows % num_worker == 0

## LU
- Compile
```
$ make lu
$ make lu_abft
```
- Run
```
$ mpirun -np <num_of_procs> bin/lu <matrix_size>
$ mpirun -np <num_of_procs> bin/lu_abft <matrix_size>
```

## K-means
- Compile
```
$ make kmeans
$ make kmeans_abft
```
- Run
```
$ mpirun -np <num_of_procs> bin/kmeans [clusters] [max_iterations] [datafile] # (default) 100 1000 ./data/obs_info.txt
$ mpirun -np <num_of_procs> bin/kmeans_abft [clusters] [max_iterations] [datafile] # (default) 100 1000 ./data/obs_info.txt
```