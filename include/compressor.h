#include <stdint.h>

#define ALG_NONE 0
#define ALG_RLE 1
#define ALG_HUFFMAN 2

#define PROTECTION_NONE 0
#define PROTECTION_XOR 1

#define MAX_HUFFMAN_DEPTH 256

extern const uint8_t SIGNATURE[6];
extern const uint8_t VERSION[2];
extern const uint8_t NO_CONTEXT_ALG;
extern const uint8_t CONTEXT_ALG;
extern const uint8_t PROTECTION;

typedef struct RLE_pair {
  uint8_t L;
  uint8_t c;
} RLE_pair;

typedef struct frequency_pair {
  uint16_t pair;
  unsigned long frequency;
} frequency_pair;

typedef struct node_huffman {
  uint16_t pair;
  unsigned long frequency;
  uint8_t node_type; // 1 - leaf, 0 - internal
  struct node_huffman *left;
  struct node_huffman *right;
} node_huffman;

typedef struct code_huffman {
  uint16_t pair;
  uint8_t *bits;
  uint16_t length;
} code_huffman;

typedef struct MinHeap {
  node_huffman **nodes;
  int size;
  int capacity;
} MinHeap;

// MinHeap
MinHeap *create_minheap(int capacity);
void destroy_minheap(MinHeap *heap);
void minheap_insert(MinHeap *heap, node_huffman *node);
node_huffman *minheap_extract_min(MinHeap *heap);

// Huffman tree
node_huffman *create_leaf_node(uint16_t pair, unsigned long frequency);
node_huffman *create_internal_node(node_huffman *left, node_huffman *right);
node_huffman *build_huffman_tree(MinHeap *heap);
uint16_t decode_symbol(node_huffman *root, uint8_t *buffer, int size,
                       int *current_byte, uint8_t *current_bit,
                       uint8_t last_byte_bits);
void destroy_huffman_tree(node_huffman *root);

// Huffman code
void generate_huffman_codes(node_huffman *root, code_huffman *code_table,
                            int depth, int code_table_size);
int find_code_index(code_huffman *code_table, uint16_t pair,
                    int code_table_size);

// utils
void set_bit(uint8_t *bit_array, int bit_position, uint8_t value);
void apply_xor_protection(uint8_t *data, int size, uint8_t key);
uint8_t read_bit_from_buffer(uint8_t *buffer, int size, int *current_byte,
                             uint8_t *current_bit);

// encode/decode
void encode_file(const char *input_path, const char *output_path);
void decode_file(const char *input_path, const char *output_path);
