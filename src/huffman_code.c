#include "../include/compressor.h"
#include <stdlib.h>
#include <string.h>

// Huffman codes
uint8_t current_code[(MAX_HUFFMAN_DEPTH + 7) / 8] = {0};

void save_code(code_huffman *code_entry, uint16_t pair, int depth) {
  code_entry->pair = pair;
  code_entry->length = depth;

  int bytes_needed = (depth + 7) / 8;
  code_entry->bits = malloc(bytes_needed);
  memcpy(code_entry->bits, current_code, bytes_needed);
}

int find_code_index(code_huffman *code_table, uint16_t pair,
                    int code_table_size) {
  for (int i = 0; i < code_table_size; i++) {
    if (code_table[i].pair == 0 || code_table[i].pair == pair) {
      return i;
    }
  }
  return -1;
}

void generate_huffman_codes(node_huffman *root, code_huffman *code_table,
                            int depth, int code_table_size) {
  if (root == NULL)
    return;

  if (root->node_type == 1) {
    int index = find_code_index(code_table, root->pair, code_table_size);
    save_code(&code_table[index], root->pair, depth);
    return;
  }

  set_bit(current_code, depth, 1);
  generate_huffman_codes(root->right, code_table, depth + 1, code_table_size);

  set_bit(current_code, depth, 0);
  generate_huffman_codes(root->left, code_table, depth + 1, code_table_size);
}
