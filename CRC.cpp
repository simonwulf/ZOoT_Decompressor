// CRC.h

/*
  Shamelessly borrowed from the more general version found at http://n64dev.org/n64crc.html
*/

#include "CRC.h"

#define ROL(i, b) (((i) << (b)) | ((i) >> (32 - (b))))
#define BYTES2LONG(b) ( (b)[0] << 24 | \
                        (b)[1] << 16 | \
                        (b)[2] << 8 | \
                        (b)[3] )

#define N64_HEADER_SIZE  0x40

#define CHECKSUM_CIC6105 0xDF26F436
#define CHECKSUM_START   0x00001000
#define CHECKSUM_LENGTH  0x00100000

int N64CalcCRC(unsigned int *crc, unsigned char *data) {
  int bootcode, i;

  unsigned int t1, t2, t3;
  unsigned int t4, t5, t6;
  unsigned int r, d;

  t1 = t2 = t3 = t4 = t5 = t6 = CHECKSUM_CIC6105;

  i = CHECKSUM_START;
  while (i < (CHECKSUM_START + CHECKSUM_LENGTH)) {
    d = BYTES2LONG(&data[i]);
    if ((t6 + d) < t6) t4++;
    t6 += d;
    t3 ^= d;
    r = ROL(d, (d & 0x1F));
    t5 += r;
    if (t2 > d) t2 ^= r;
    else t2 ^= t6 ^ d;

    t1 += BYTES2LONG(&data[N64_HEADER_SIZE + 0x0710 + (i & 0xFF)]) ^ d;

    i += 4;
  }

  crc[0] = t6 ^ t4 ^ t3;
  crc[1] = t5 ^ t2 ^ t1;

  return 0;
}
