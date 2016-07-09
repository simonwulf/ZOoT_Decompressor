// Yaz0.cpp

/**
 * Implementation of Yaz0 decompression as documented here:
 * http://www.amnoid.de/gc/yaz0.txt
 * http://wiki.tockdom.com/wiki/YAZ0_(File_Format)
 */

#include <stdint.h>

#include "Yaz0.h"

static uint32_t endianSwap32(uint32_t value) {
  return (value & 0x000000ff) << 24 |
    (value & 0x0000ff00) << 8 |
    (value & 0x00ff0000) >> 8 |
    (value & 0xff000000) >> 24;
}

struct YAZ0Header {
  uint8_t YAZ0[4];
  uint32_t decomp_size;
  uint8_t padding[8];
};

static struct YAZ0Header readYAZ0Header(uint8_t* data) {
  YAZ0Header header = *(YAZ0Header*)data;
  header.decomp_size = endianSwap32(header.decomp_size);
  return header;
}

int yaz0_decompress(uint8_t* buffer, uint8_t* compressed, unsigned int decomp_size) {
  uint8_t* caret = compressed;
  uint8_t* buffer_caret = buffer;
  YAZ0Header header = readYAZ0Header(caret);

  for (int i = 0; i < 4; i++) {
    if (header.YAZ0[i] != "Yaz0"[i])
      return YAZ0_INVALID_HEADER;
  }

  if (header.decomp_size != decomp_size) // ZOoT stores the decompressed size in it's file system, so I can do this check. Not so necessary though.
    return YAZ0_SIZE_MISMATCH;

  caret += sizeof(YAZ0Header);

  int group_count = 0;

  while (buffer_caret < (buffer + decomp_size)) {
    uint8_t group_header = *(caret++);
    group_count++;

    for (int i = 7; i >= 0; i--) {

      if (group_header & (1 << i)) {
        if (buffer_caret == buffer + decomp_size)
          return YAZ0_CORRUPT; // Assuming this should be a fail, should it though?
        *(buffer_caret++) = *(caret++);
      } else {

        uint8_t N;
        uint16_t R;
        int copy_size;

        if (*caret & 0xf0) {
          // NR RR
          N = (*caret & 0xf0) >> 4;
          R = ((*caret) & 0x0f) << 8 | *(caret + 1);
          copy_size = N + 2;
          caret += 2;
        } else {
          // 0R RR NN
          N = *(caret + 2);
          R = ((*caret) & 0x0f) << 8 | *(caret + 1);
          copy_size = N + 18;
          caret += 3;
        }

        uint8_t* buffer_copy = buffer_caret - (R + 1);

        while (copy_size-- > 0 && buffer_caret < buffer + decomp_size) {
          *(buffer_caret++) = *(buffer_copy++);
        }

        if (buffer_caret >= (buffer + decomp_size))
          break;
      }
    }
  }

  return YAZ0_OK;
}
