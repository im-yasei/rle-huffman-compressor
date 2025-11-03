#include "../include/compressor.h"

const uint8_t SIGNATURE[6] = {0x72, 0x6C, 0x68, 0x66, 0x6D, 0x6E};
const uint8_t VERSION[2] = {0x01, 0x00};
const uint8_t NO_CONTEXT_ALG = ALG_RLE;
const uint8_t CONTEXT_ALG = ALG_HUFFMAN;
const uint8_t PROTECTION = PROTECTION_XOR;

void set_bit(uint8_t *bit_array, int bit_position, uint8_t value) {
  int byte_index = bit_position / 8;
  int bit_index = 7 - (bit_position % 8);

  if (value == 1) {
    bit_array[byte_index] |= (1 << bit_index);
  } else {
    bit_array[byte_index] &= ~(1 << bit_index);
  }
}

// XOR protection
void apply_xor_protection(uint8_t *data, int size, uint8_t key) {
  for (int i = 0; i < size; i++) {
    data[i] ^= key;
  }
}
