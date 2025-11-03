#include "../include/compressor.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void encode_file(const char *input_path, const char *output_path) {

  RLE_pair *rle_encode = NULL;
  frequency_pair *frequency_table = NULL;

  unsigned long frequency[] = {0};

  uint8_t buffer[4096];
  unsigned char temp;
  size_t bytes_read;
  int byte;
  int total_bytes = 0;
  int rle_size = 0, frequency_size = 0;
  uint8_t repeat_counter;
  size_t data_size = 0;
  uint8_t xor_key = 0xAB;

  FILE *fr = fopen(input_path, "rb");
  FILE *fw = fopen(output_path, "wb");

  fseek(fr, 0, SEEK_END);
  uint64_t file_size = ftell(fr);
  fseek(fr, 0, SEEK_SET);

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), fr)) > 0) {
    for (size_t i = 0; i < bytes_read; i++) {
      repeat_counter = 1;

      while (((i + repeat_counter) < bytes_read) &&
             (buffer[i + repeat_counter] == buffer[i]) &&
             (repeat_counter < 255)) {
        repeat_counter++;
      }

      rle_encode = realloc(rle_encode, (rle_size + 1) * sizeof(RLE_pair));
      rle_encode[rle_size].L = repeat_counter;
      rle_encode[rle_size].c = buffer[i];
      rle_size++;

      i += repeat_counter - 1;
    }
  }

  for (int i = 0; i < rle_size; i++) {
    uint16_t current_pair =
        ((uint16_t)rle_encode[i].L << 8) | ((uint16_t)rle_encode[i].c);

    int found_index = -1;

    for (int j = 0; j < frequency_size; j++) {
      if (frequency_table[j].pair == current_pair) {
        found_index = j;
        break;
      }
    }

    if (found_index != -1) {
      frequency_table[found_index].frequency++;
    } else {
      frequency_table = realloc(frequency_table,
                                (frequency_size + 1) * sizeof(frequency_pair));
      frequency_table[frequency_size].pair = current_pair;
      frequency_table[frequency_size].frequency = 1;
      frequency_size++;
    }
  }

  MinHeap *heap = create_minheap(frequency_size);

  for (int i = 0; i < frequency_size; i++) {
    minheap_insert(heap, create_leaf_node(frequency_table[i].pair,
                                          frequency_table[i].frequency));
  }

  node_huffman *root = build_huffman_tree(heap);

  int depth = 0;

  code_huffman *code_table = malloc(sizeof(code_huffman) * frequency_size);
  generate_huffman_codes(root, code_table, depth, frequency_size);

  uint8_t bit_buffer = 0;
  int bit_count = 0;
  int main_buffer_size = 0;
  int main_buffer_capacity = 256;
  uint8_t *main_buffer = malloc(main_buffer_capacity);

  for (int i = 0; i < rle_size; i++) {

    uint16_t current_pair = (rle_encode[i].L << 8) | rle_encode[i].c;
    int code_index = find_code_index(code_table, current_pair, frequency_size);
    code_huffman *code = &code_table[code_index];

    for (int bit_pos = 0; bit_pos < code->length; bit_pos++) {
      int byte_index = bit_pos / 8;
      int bit_index = 7 - (bit_pos % 8);
      int bit = (code->bits[byte_index] >> bit_index) & 1;

      bit_buffer = (bit_buffer << 1) | bit;
      bit_count++;

      if (bit_count == 8) {
        if (main_buffer_size >= main_buffer_capacity) {
          main_buffer_capacity *= 2;
          main_buffer = realloc(main_buffer, main_buffer_capacity);
        }

        main_buffer[main_buffer_size++] = bit_buffer;
        bit_buffer = 0;
        bit_count = 0;
      }
    }
  }

  if (bit_count > 0) {
    bit_buffer <<= (8 - bit_count);
    if (main_buffer_size >= main_buffer_capacity) {
      main_buffer_capacity++;
      main_buffer = realloc(main_buffer, main_buffer_capacity);
    }
    main_buffer[main_buffer_size++] = bit_buffer;
  }

  apply_xor_protection(main_buffer, main_buffer_size, xor_key);

  fwrite(SIGNATURE, sizeof(uint8_t), 6, fw);
  fwrite(VERSION, sizeof(uint8_t), 2, fw);
  fwrite(&NO_CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&PROTECTION, sizeof(uint8_t), 1, fw);
  fwrite(&file_size, sizeof(uint64_t), 1, fw);
  fwrite(&frequency_size, sizeof(int), 1, fw);

  for (int i = 0; i < frequency_size; i++) {
    fwrite(&code_table[i].pair, sizeof(uint16_t), 1, fw);
    fwrite(&code_table[i].length, sizeof(uint16_t), 1, fw);
    fwrite(code_table[i].bits, sizeof(uint8_t), (code_table[i].length + 7) / 8,
           fw);
  }

  fwrite(&main_buffer_size, sizeof(uint32_t), 1, fw);
  fwrite(main_buffer, sizeof(uint8_t), main_buffer_size, fw);

  free(rle_encode);
  free(frequency_table);
  destroy_minheap(heap);
  free(root);

  for (int i = 0; i < frequency_size; i++) {
    if (code_table[i].bits != NULL) {
      free(code_table[i].bits);
    }
  }
  free(code_table);

  free(main_buffer);

  fclose(fr);
  fclose(fw);
}

// Temporary implementation: only reads file signature and compressed data
// TODO: Add full Huffman and RLE decoding
void decode_file(const char *input_path, const char *output_path) {
  FILE *fr = fopen(input_path, "rb");
  FILE *fw = fopen(output_path, "wb");

  uint8_t read_signature[6];
  uint8_t read_version[2];
  uint8_t no_context_alg;
  uint8_t context_alg;
  uint8_t protection;
  uint8_t xor_key = 0xAB;

  int file_size, frequency_size, main_buffer_size, total_read = 0;

  code_huffman *code_table = NULL;
  uint8_t *main_buffer = NULL;

  fread(read_signature, sizeof(uint8_t), 6, fr);
  fread(read_version, sizeof(uint8_t), 2, fr);
  fread(&no_context_alg, sizeof(uint8_t), 1, fr);
  fread(&context_alg, sizeof(uint8_t), 1, fr);
  fread(&protection, sizeof(uint8_t), 1, fr);
  fread(&file_size, sizeof(int), 1, fr);
  fread(&frequency_size, sizeof(int), 1, fr);

  code_table = malloc(frequency_size * sizeof(code_huffman));

  for (int i = 0; i < frequency_size; i++) {
    fread(&code_table[i].pair, sizeof(uint16_t), 1, fr);
    fread(&code_table[i].length, sizeof(uint16_t), 1, fr);
    fread(code_table[i].bits, sizeof(uint8_t), (code_table[i].length + 7) / 8,
          fr);
  }

  fread(&main_buffer_size, sizeof(int), 1, fr);

  main_buffer = malloc(main_buffer_size * sizeof(uint8_t));

  fread(main_buffer, sizeof(uint8_t), main_buffer_size, fr);

#if 0
  uint8_t *buffer = malloc(sizeof(uint8_t) * 8);
  uint16_t *rle_pairs_buffer = NULL;

  for (int i = 0; i < 256; i++) {
  }

  if ((memcmp(read_signature, SIGNATURE, 6) != 0) ||
      (memcmp(read_version, VERSION, 2) != 0)) {
    printf("error\n");
  }

  // uint8_t buffer[4096];
  size_t bytes_read;

  while (total_read < file_size &&
         (bytes_read = fread(buffer, 1, sizeof(buffer), fr)) > 0) {
    fwrite(buffer, 1, bytes_read, fw);
    total_read += bytes_read;
  }

  fclose(fr);
  fclose(fw);
#endif
}

int main() {
#if 1
  encode_file("test.txt", "code.rlhfmn");
#endif

#if 0
  decode_file("code.rlhfmn", "decode.txt");
#endif
  return 0;
}
