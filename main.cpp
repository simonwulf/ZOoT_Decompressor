// main.cpp

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "Yaz0.h"
#include "CRC.h"

uint32_t endianSwap32(uint32_t value) {
  return (value & 0x000000ff) << 24 |
         (value & 0x0000ff00) << 8 |
         (value & 0x00ff0000) >> 8 |
         (value & 0xff000000) >> 24;
}

struct FileOffsetEntry {
  uint32_t virtual_start;
  uint32_t virtual_end;
  uint32_t physical_start;
  uint32_t physical_end;
};

/**
 * Takes care of byte swapping when reading the big endian rom
 */
FileOffsetEntry readFOEntry(uint8_t* data) {
  FileOffsetEntry entry = *(FileOffsetEntry*)data;
  entry.virtual_start = endianSwap32(entry.virtual_start);
  entry.virtual_end = endianSwap32(entry.virtual_end);
  entry.physical_start = endianSwap32(entry.physical_start);
  entry.physical_end = endianSwap32(entry.physical_end);
  return entry;
}

/**
 * Same as above but for writing
 */
void writeFOEntry(uint8_t* dst, FileOffsetEntry fo_entry) {
  FileOffsetEntry* dst_entry = (FileOffsetEntry*)dst;
  (*dst_entry).virtual_start = endianSwap32(fo_entry.virtual_start);
  (*dst_entry).virtual_end = endianSwap32(fo_entry.virtual_end);
  (*dst_entry).physical_start = endianSwap32(fo_entry.physical_start);
  (*dst_entry).physical_end = endianSwap32(fo_entry.physical_end);
}

/**
 * Locates the find offset table by searching for "zelda@" and adding 0x30 bytes.
 * "zelda@" is not actually part of the file offset table, but appears right at
 * the end of the file that comes just before it.
 */
uint8_t* findFileOffsetTable(uint8_t* data, unsigned int size) {

  const char* search_string = "zelda@";
  bool found = false;
  uint8_t* table = data;

  while (!found && table != data + size) {
    found = true;
    for (int i = 0; i < 6; i++) {
      if (table[i] != search_string[i]) {
        found = false;
        break;
      }
    }
    if (!found)
      table++;
  }

  if (found)
    return table + 0x30;
  else
    return NULL;
}

/**
 * Calculates the amount of memory that needs to be allocated in order to hold the
 * decompressed rom by simply finding the highest end address in the file offset
 * table. Does not bother with padding the rom to power of two mbit which I think
 * you may want to do, were you ever to put this on an actual cartridge(?).
 */
unsigned int calcDecompSize(unsigned int comp_size, uint8_t* file_offset_table) {

  unsigned int size = 0;

  while (true) {
    FileOffsetEntry file_offset = readFOEntry(file_offset_table);
    if (file_offset.virtual_start == 0 && file_offset.virtual_end == 0)
      break;

    if (file_offset.virtual_end > size)
      size = file_offset.virtual_end;
    file_offset_table += sizeof(FileOffsetEntry);
  }

  printf("Compressed: %x\tDecompressed: %x\n", comp_size, size);
  return size;
}

/**
 * Decompresses the rom. This is just broken out to be tidy, the actual decompression
 * takes place in Yaz0.cpp.
 * The ZOoT file system is documented here: http://wiki.spinout182.com/w/Zelda_64_Filesystem
 */
int decompress(uint8_t* comp_rom, uint8_t* file_offset_table, uint8_t* buffer, unsigned int size) {

  uint8_t* comp_fo_table = file_offset_table;
  uint8_t* decomp_fo_table;

  unsigned int warnings = 0;

  while (true) {
    FileOffsetEntry file_offset = readFOEntry(comp_fo_table);
    if (file_offset.virtual_start == 0 && file_offset.virtual_end == 0) // Detect end of file offset table
      break;

    unsigned int decomp_size = file_offset.virtual_end - file_offset.virtual_start;

    if (file_offset.physical_end == 0) {
      // File is not compressed, just copy it over
      memcpy(
        buffer + file_offset.virtual_start,
        comp_rom + file_offset.physical_start,
        decomp_size
      );
    } else {
      // File is compressed, decompress into buffer
      int result = yaz0_decompress(
        buffer + file_offset.virtual_start,
        comp_rom + file_offset.physical_start,
        decomp_size
      );
      if (result != YAZ0_OK) {
        printf(
          "Warning: %08x %08x %08x %08x did not decompress successfully\n",
          file_offset.virtual_start, file_offset.virtual_end,
          file_offset.physical_start, file_offset.physical_end
        );
        warnings++;
      }
    }

    comp_fo_table += sizeof(FileOffsetEntry);
  }

  // Update file offset table
  comp_fo_table = file_offset_table;
  decomp_fo_table = findFileOffsetTable(buffer, size);

  while (true) {
    FileOffsetEntry file_offset = readFOEntry(comp_fo_table);
    if (file_offset.virtual_start == 0 && file_offset.virtual_end == 0)
      break;

    file_offset.physical_start = file_offset.virtual_start;
    file_offset.physical_end = 0;

    writeFOEntry(decomp_fo_table, file_offset);

    decomp_fo_table += sizeof(FileOffsetEntry);
    comp_fo_table += sizeof(FileOffsetEntry);
  }

  return warnings;
}

int main(int argc, const char* argv[]) {

  if (argc == 1) {
    printf("Give me the path to your ZoOT ROM!\n");
    return 0;
  }

  const char* in_path = argv[1];
  unsigned char* data;
  unsigned int comp_size;
  unsigned int decomp_size;

  // Construct output path
  // TODO: support file names without a dot
  unsigned int type_index = strlen(in_path) - 1;
  while (in_path[type_index] != '.' && type_index > 0) {
    type_index--;
  }

  if (type_index == 0)
    type_index = strlen(in_path) - 1;

  char* out_path;
  char* suffix = "[DECOMP].z64";
  unsigned int suffix_len = strlen(suffix);
  out_path = new char[type_index + suffix_len + 1];
  memcpy(out_path, in_path, type_index + 1);
  memcpy(out_path + type_index, suffix, suffix_len + 1);

  // Read compressed rom into memory
  {
    FILE* file = fopen(in_path, "rb");
    if (file == NULL) {
      printf("Error: Could not open file %s for reading\n", in_path);
      return 1;
    }

    fseek(file, 0, SEEK_END);
    comp_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = new unsigned char[comp_size];
    fread(data, 1, comp_size, file);
    fclose(file);
  }

  // TODO: detect rom endianness and swap accordingly

  // Find file offset table by searching for "zelda@"
  uint8_t* file_offset_table = findFileOffsetTable(data, comp_size);

  if (file_offset_table == NULL) {
    printf("Error: could not find file offset table\n");
    return 1;
  }

  printf("File offset table found at 0x%08x\n", (file_offset_table - data));

  // Calculate size needed for decompressed rom
  decomp_size = calcDecompSize(comp_size, file_offset_table);
  uint8_t* decomp_rom = new uint8_t[decomp_size];
  memset(decomp_rom, 0x00, decomp_size); // Filling the output ROM with all zeros, don't wanna accidentally write any lingering data to the file

  // Decompress into decomp_rom
  printf("Decompressing...\n");
  unsigned int result = decompress(data, file_offset_table, decomp_rom, decomp_size);
  if (result > 0)
    printf("There were %i warnings. If you're lucky, the rom might still boot though :)\n", result);
  else
    printf("Successfully decompressed %s\n", in_path);

  // Update CRC, the game will refuse to boot without this.
  // Learn more here: http://www.emutalk.net/threads/53938-N64-tech-documentation?p=445417&viewfull=1#post445417
  // And here: https://www.youtube.com/watch?v=HwEdqAb2l50
  uint32_t crc[2];
  N64CalcCRC(crc, decomp_rom);
  *(uint32_t*)(decomp_rom + 0x10) = endianSwap32(crc[0]);
  *(uint32_t*)(decomp_rom + 0x14) = endianSwap32(crc[1]);

  // Write the decompressed rom
  {
    FILE* file = fopen(out_path, "wb");
    if (file == NULL) {
      printf("Error: Could not open file %s for writing\n", in_path);
      return 1;
    }

    fwrite(decomp_rom, 1, decomp_size, file);
    fclose(file);
  }

  // Intentionally leaking these
  // delete[] data;
  // delete[] decomp_rom;
  // delete[] out_path;

  return 0;
}
