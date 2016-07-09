// Yaz0.h

#ifndef YAZ0_H_INCLUDED
#define YAZ0_H_INCLUDED

#define YAZ0_OK 0
#define YAZ0_INVALID_HEADER 1
#define YAZ0_SIZE_MISMATCH 2
#define YAZ0_CORRUPT 3

int yaz0_decompress(uint8_t* buffer, uint8_t* compressed, unsigned int decomp_size);

#endif
