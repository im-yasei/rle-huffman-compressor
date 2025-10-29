#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALG_NONE 0
#define ALG_RLE 1
#define ALG_HUFFMAN 2

#define PROTECTION_NONE 0
#define PROTECTION_XOR 1

const uint8_t SIGNATURE[6] = {0x72, 0x6C, 0x68, 0x66, 0x6D, 0x6E};
const uint8_t VERSION[2] = {0x01, 0x00};
const uint8_t NO_CONTEXT_ALG = ALG_RLE;
const uint8_t CONTEXT_ALG = ALG_HUFFMAN;
const uint8_t PROTECTION = PROTECTION_XOR;

typedef struct {
  uint8_t L;
  uint8_t c;
} RLE_pair;

typedef struct {
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

typedef struct {
  uint16_t pair;
  uint8_t *bits;
  uint8_t length;
} code_huffman;

typedef struct {
  node_huffman **nodes;
  int size;
  int capacity;
} MinHeap;

int compare_frequency(const void *a, const void *b) {
  const frequency_pair *fa = (const frequency_pair *)a;
  const frequency_pair *fb = (const frequency_pair *)b;

  if (fa->frequency < fb->frequency)
    return -1;
  if (fa->frequency > fb->frequency)
    return 1;

  if (fa->pair < fb->pair)
    return -1;
  if (fa->pair > fb->pair)
    return 1;

  return 0;
}

// for Min Heap
MinHeap *create_minheap(int capacity) {
  MinHeap *heap = malloc(sizeof(MinHeap));
  if (heap == NULL) {
    return NULL;
  }

  heap->size = 0;
  heap->capacity = capacity;

  heap->nodes = malloc(capacity * sizeof(node_huffman *));
  if ((heap->nodes) == NULL) {
    free(heap);
    return NULL;
  }

  return heap;
}

void destroy_minheap(MinHeap *heap) {
  free(heap->nodes);
  free(heap);
}

int parent(int i) { return ((i - 1) / 2); }
int left_child(int i) { return (2 * i + 1); }
int right_child(int i) { return (2 * i + 2); }
void swap_nodes(node_huffman **a, node_huffman **b) {
  node_huffman *temp = *a;
  *a = *b;
  *b = temp;
}

void minheap_heapify(MinHeap *heap, int i) {
  int smallest = i;
  int left = left_child(i);
  int right = right_child(i);

  if (left < heap->size) {
    if (heap->nodes[left]->frequency < heap->nodes[smallest]->frequency) {
      smallest = left;
    } else if (heap->nodes[left]->frequency ==
               heap->nodes[smallest]->frequency) {
      if (heap->nodes[left]->pair < heap->nodes[smallest]->pair) {
        smallest = left;
      } else if (heap->nodes[left]->node_type == 0) {
        smallest = left;
      }
    }
  }

  if (right < heap->size) {
    if (heap->nodes[right]->frequency < heap->nodes[smallest]->frequency) {
      smallest = right;
    } else if (heap->nodes[right]->frequency ==
               heap->nodes[smallest]->frequency) {
      if (heap->nodes[right]->pair < heap->nodes[smallest]->pair) {
        smallest = right;
      } else if (heap->nodes[right]->node_type == 0) {
        smallest = right;
      }
    }
  }

  if (smallest != i) {
    swap_nodes(&(heap->nodes[i]), &(heap->nodes[smallest]));
    minheap_heapify(heap, smallest);
  }
}

void minheap_insert(MinHeap *heap, node_huffman *node) {
  if (heap->capacity > heap->size) {
    heap->nodes[heap->size] = node;
    heap->size++;
  } else {
    heap->capacity *= 2;
    heap->nodes = realloc(heap->nodes, heap->capacity * sizeof(node_huffman *));
    heap->nodes[heap->size] = node;
    heap->size++;
  }
  int i = (heap->size) - 1;
  while (i > 0 &&
         heap->nodes[parent(i)]->frequency > heap->nodes[i]->frequency) {
    swap_nodes(&heap->nodes[i], &heap->nodes[parent(i)]);
    i = parent(i);
  }
}

node_huffman *minheap_extract_min(MinHeap *heap) {
  if (heap->size == 0)
    return NULL;

  node_huffman *min_node = heap->nodes[0];

  heap->nodes[0] = heap->nodes[heap->size - 1];
  heap->size--;

  minheap_heapify(heap, 0);

  return min_node;
}

// for Huffman tree
node_huffman *create_leaf_node(uint16_t pair, unsigned long frequency) {
  node_huffman *leaf = malloc(sizeof(node_huffman));
  if (leaf == NULL)
    return NULL;

  leaf->pair = pair;
  leaf->frequency = frequency;
  leaf->node_type = 1; // 1 - leaf, 0 - internal
  leaf->left = NULL;
  leaf->right = NULL;
  return leaf;
}

node_huffman *create_internal_node(node_huffman *left, node_huffman *right) {
  node_huffman *internal = malloc(sizeof(node_huffman));
  if (internal == NULL)
    return NULL;

  internal->pair = 0;
  internal->frequency = left->frequency + right->frequency;
  internal->node_type = 0; // 1 - leaf, 0 - internal
  internal->left = left;
  internal->right = right;
  return internal;
}

node_huffman *build_huffman_tree(MinHeap *heap) {
  while (heap->size > 1) {
    node_huffman *left = minheap_extract_min(heap);
    node_huffman *right = minheap_extract_min(heap);

    node_huffman *internal = create_internal_node(left, right);

    minheap_insert(heap, internal);
  }
  return minheap_extract_min(heap);
}

// Only RLE encoding implemented
void encode_file(const char *input_path, const char *output_path) {

  RLE_pair *rle_encode = NULL;
  frequency_pair *frequency_table = NULL;

  unsigned long frequency[] = {0};

  uint8_t buffer[4096];
  unsigned char temp;
  size_t bytes_read;
  int byte;
  int total_bytes = 0;
  int unique_count = 0, rle_size = 0, frequency_size = 0;
  uint8_t repeat_counter;

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

  free(rle_encode);

  qsort(frequency_table, frequency_size, sizeof(frequency_pair),
        compare_frequency);

  fwrite(SIGNATURE, sizeof(uint8_t), 6, fw);
  fwrite(VERSION, sizeof(uint8_t), 2, fw);
  fwrite(&file_size, sizeof(uint64_t), 1, fw);
  fwrite(&NO_CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&CONTEXT_ALG, sizeof(uint8_t), 1, fw);
  fwrite(&PROTECTION, sizeof(uint8_t), 1, fw);

  fclose(fr);
  fclose(fw);
}

// This just copies data without actual decoding
// Real implementation will include Huffman + RLE decoding
#if 0
void decode_file(const char *input_path, const char *output_path) {
  FILE *fr = fopen(input_path, "rb");
  FILE *fw = fopen(output_path, "wb");

  uint8_t read_signature[6];
  uint8_t read_version[2];
  uint64_t file_size, total_read = 0;

  fread(read_signature, sizeof(uint8_t), 6, fr);
  fread(read_version, sizeof(uint8_t), 2, fr);
  fread(&file_size, sizeof(uint64_t), 1, fr);

  if ((memcmp(read_signature, SIGNATURE, 6) != 0) ||
      (memcmp(read_version, VERSION, 2) != 0)) {
    printf("error\n");
  }

  uint8_t buffer[4096];
  size_t bytes_read;

  while (total_read < file_size &&
         (bytes_read = fread(buffer, 1, sizeof(buffer), fr)) > 0) {
    fwrite(buffer, 1, bytes_read, fw);
    total_read += bytes_read;
  }

  fclose(fr);
  fclose(fw);
}
#endif

int main() {
#if 1
  encode_file("Q.txt", "code.otik");
#endif

#if 0
  decode_file("code.otik", "Q_decode.txt");
#endif
  return 0;
}
