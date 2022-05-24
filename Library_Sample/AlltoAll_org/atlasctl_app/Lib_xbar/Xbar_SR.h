#include "atlasctl.h"
int OPTWEB_MPI_Reset(void);
int OPTWEB_MPI_Barrier(void);
void OPTWEB_MPI_Wait(void);
int OPTWEB_MPI_Send(unsigned int input_data[], int data_num, MPI_Datatype sendtype, int  to_send_rank, int tag, MPI_Comm comm);
int OPTWEB_MPI_Recv(unsigned int output_data[],  int data_num, MPI_Datatype recvtype, int  from_send_rank
                        , int tag,  MPI_Comm comm, MPI_Status* status);
int OPTWEB_MPI_Sendrecv(unsigned int input_data[], int data_num, MPI_Datatype sendtype, int  target_rank
                        , unsigned int output_data[], int data_num2, MPI_Datatype recvtype
                        , int source, int tag, MPI_Comm comm, MPI_Status * status);
int OPTWEB_MPI_Alltoall(unsigned int input_data[], unsigned int data_num,  MPI_Datatype sendtype
                        ,unsigned int output_data[], unsigned int data_num2, MPI_Datatype recvtype, MPI_Comm comm);
int OPTWEB_MPI_Allgather(unsigned int input_data[], int data_num,  MPI_Datatype sendtype
                        ,unsigned int output_data[], int data_num2, MPI_Datatype recvtype, MPI_Comm comm);
int OPTWEB_MPI_Scatter(unsigned int input_data[], int data_num, MPI_Datatype sendtype
                        ,unsigned int output_data[], int data_num2, MPI_Datatype recvtype
                        ,int send_rank,  MPI_Comm comm);
int OPTWEB_MPI_Gather(unsigned int input_data[], int data_num, MPI_Datatype sendtype
                        , unsigned int output_data[], int data_num2
                        , MPI_Datatype recvtype, int recv_rank, MPI_Comm comm);
int OPTWEB_MPI_Bcast(unsigned int buf_data[], int data_num, MPI_Datatype datatype, int send_rank, MPI_Comm comm);
int OPTWEB_MPI_Init(int* argc, char*** argv);
void OPTWEB_MPI_Comm_rank(MPI_Comm comm, int* rank);
void OPTWEB_MPI_Finalize(void);
int ioctrl_execute_do_dma_read2(struct atlas_cmd *atctrl,unsigned char *buf);

