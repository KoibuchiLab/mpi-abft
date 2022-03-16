#ifndef  _ABFT_H_ 
#define _ABFT_H_

void MPI_Bcast_abft(double *buffer, int count, int root, int rank, int procs); //, int* resend);
uint32_t do_crc32(unsigned char *data_bits, int bytes);
int block_size(int data_bytes); //calculate block size (bytes)
void cast_bits_to_char(unsigned char* bits, char* data, int bytes); //1 bit --> char '0' or '1' (8 bits)
int hmLength(int k); //hamming check bits
void hamming_code(char* data, char* c, int k, int r); //value of each hamming check bit
void hamming_verify_bit(unsigned char* bits, char* c, int bytes, int r, char* v);
void hamming_rectify_bit(unsigned char* bits, char* c, int bytes, int r, int error_bit_pos); //rectify error bit if one error (except parity bit)
int error_info(char* v, int r, int* error_bit_pos); //calculate bit error position if possible one bit error
void hamming_encode(unsigned char* bits, char** c, int bytes, int* r); //data bits --> char[]
int hamming_decode(unsigned char* bits, char* c, int bytes, int r);
void bit_flip(unsigned char* bits, int bytes); //bit flip

#endif