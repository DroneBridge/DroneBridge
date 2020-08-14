#pragma once
#include <stdint.h>

typedef struct fec_parms *fec_code_t;

/*
 * create a new encoder, returning a descriptor. This contains k,n and
 * the encoding matrix.
 * n is the number of data blocks + fec blocks (matrix height)
 * k is just the data blocks (matrix width)
 */
void fec_init_old(void);

void fec_encode_old(unsigned int blockSize,
                unsigned char **data_blocks,
                unsigned int nrDataBlocks,
                unsigned char **fec_blocks,
                unsigned int nrFecBlocks);

void fec_decode_old(unsigned int blockSize,
                unsigned char **data_blocks,
                unsigned int nr_data_blocks,
                unsigned char **fec_blocks,
                unsigned int *fec_block_nos,
                unsigned int *erased_blocks,
                unsigned short nr_fec_blocks  /* how many blocks per stripe */);

void fec_print(fec_code_t code, int width);

void fec_license_old(void);

void slow_mul1(uint8_t *dst1, uint8_t *src1, uint8_t c, unsigned int sz);

void slow_addmul1(uint8_t *dst1, uint8_t *src1, uint8_t c, int sz);

int invert_mat(uint8_t *src, unsigned int k);
