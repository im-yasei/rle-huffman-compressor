#include "../include/compressor.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

  uint8_t bit_buffer = 0, last_byte_bits;
  int bit_count = 0;
  uint32_t main_buffer_size = 0;
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

  last_byte_bits = bit_count;

  apply_xor_protection(main_buffer, main_buffer_size, xor_key);

  fwrite(SIGNATURE, sizeof(uint8_t), 6, fw);
  fwrite(VERSION, sizeof(uint8_t), 2, fw);
  fwrite(&NO_CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&PROTECTION, sizeof(uint8_t), 1, fw);
  fwrite(&file_size, sizeof(uint64_t), 1, fw);
  fwrite(&frequency_size, sizeof(int), 1, fw);

  for (int i = 0; i < frequency_size; i++) {
    fwrite(&frequency_table[i].pair, sizeof(uint16_t), 1, fw);
    fwrite(&frequency_table[i].frequency, sizeof(unsigned long), 1, fw);
  }

  fwrite(&last_byte_bits, sizeof(uint8_t), 1, fw);
  fwrite(&main_buffer_size, sizeof(uint32_t), 1, fw);
  fwrite(main_buffer, sizeof(uint8_t), main_buffer_size, fw);

  free(rle_encode);
  free(frequency_table);
  destroy_minheap(heap);
  destroy_huffman_tree(root);

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

void decode_file(const char *input_path, const char *output_path) {
  FILE *fr = fopen(input_path, "rb");
  FILE *fw = fopen(output_path, "wb");

  uint8_t read_signature[6];
  uint8_t read_version[2];
  uint8_t no_context_alg;
  uint8_t context_alg;
  uint8_t protection;
  uint8_t xor_key = 0xAB;
  uint8_t last_byte_bits;
  uint64_t file_size;
  uint32_t main_buffer_size = 0;

  int frequency_size, total_read = 0;
  int decoded_count = 0;
  unsigned long rle_size = 0;

  uint8_t *main_buffer = NULL;

  fread(read_signature, sizeof(uint8_t), 6, fr);
  fread(read_version, sizeof(uint8_t), 2, fr);
  fread(&no_context_alg, sizeof(uint8_t), 1, fr);
  fread(&context_alg, sizeof(uint8_t), 1, fr);
  fread(&protection, sizeof(uint8_t), 1, fr);
  fread(&file_size, sizeof(uint64_t), 1, fr);
  fread(&frequency_size, sizeof(int), 1, fr);

  frequency_pair *frequency_table =
      malloc(frequency_size * sizeof(frequency_pair));

  for (int i = 0; i < frequency_size; i++) {
    fread(&frequency_table[i].pair, sizeof(uint16_t), 1, fr);
    fread(&frequency_table[i].frequency, sizeof(unsigned long), 1, fr);
  }

  fread(&last_byte_bits, sizeof(uint8_t), 1, fr);

  fread(&main_buffer_size, sizeof(uint32_t), 1, fr);

  main_buffer = malloc(main_buffer_size * sizeof(uint8_t));

  fread(main_buffer, sizeof(uint8_t), main_buffer_size, fr);

  apply_xor_protection(main_buffer, main_buffer_size, xor_key);

  // Decoding wiht Huffman tree
  MinHeap *heap = create_minheap(frequency_size);

  for (int i = 0; i < frequency_size; i++) {
    minheap_insert(heap, create_leaf_node(frequency_table[i].pair,
                                          frequency_table[i].frequency));
    rle_size += frequency_table[i].frequency;
  }

  node_huffman *root = build_huffman_tree(heap);

  RLE_pair *rle_file = malloc(rle_size * sizeof(RLE_pair));

  int current_byte = 0;
  uint8_t current_bit = 0;

  while (decoded_count < rle_size) {
    uint16_t pair = decode_symbol(root, main_buffer, main_buffer_size,
                                  &current_byte, &current_bit, last_byte_bits);

    if (pair == 0 && current_byte >= main_buffer_size) {
      break;
    }

    rle_file[decoded_count].L = pair >> 8;
    rle_file[decoded_count].c = pair & 0xFF;

    decoded_count++;
  }

  destroy_minheap(heap);
  destroy_huffman_tree(root);

  for (int i = 0; i < rle_size; i++) {
    for (int j = 0; j < rle_file[i].L; j++) {
      fwrite(&rle_file[i].c, sizeof(uint8_t), 1, fw);
    }
  }

  free(frequency_table);
  free(main_buffer);
  free(rle_file);

  fclose(fr);
  fclose(fw);

  FILE *fr_test = fopen(output_path, "rb");

  fseek(fr_test, 0, SEEK_END);
  uint64_t decoded_file_size = ftell(fr_test);
  fseek(fr_test, 0, SEEK_SET);

  if (file_size == decoded_file_size) {
    printf("✓ OK (Source file size = Decompressed file size = %lu bytes)\n",
           file_size);
  } else {
    printf("✗ FAIL Source file size = %lu bytes\n       Decompressed file "
           "size = "
           "%lu bytes\n",
           file_size, decoded_file_size);
  }

  fclose(fr_test);
}

int main(int argc, char *argv[]) {

  int encode_mode = 0;
  int decode_mode = 0;
  int opt;

  while ((opt = getopt(argc, argv, "cdh")) != -1) {
    switch (opt) {
    case 'c':
      encode_mode = 1;
      break;
    case 'd':
      decode_mode = 1;
      break;
    case 'h':
      printf("Usage:\n");
      printf("  %s -c input_file output_file\n", argv[0]);
      printf("  %s -d input_file output_file\n", argv[0]);
      return 0;
    default:
      return 1;
    }
  }

  if (((encode_mode == 1) || (decode_mode == 1)) && optind + 2 == argc) {
    const char *input_file = argv[optind];
    const char *output_file = argv[optind + 1];

    if (encode_mode) {
      encode_file(input_file, output_file);
      printf("Compressed: %s -> %s\n", input_file, output_file);
    } else {
      decode_file(input_file, output_file);
      printf("Decompressed: %s -> %s\n", input_file, output_file);
    }
  } else {
    printf("Error: Use %s -c input output OR %s -d input output\n", argv[0],
           argv[0]);
    return 1;
  }
  return 0;
}
