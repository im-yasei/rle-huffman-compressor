#include "../include/compressor.h"
#include <stdint.h>
#include <stdlib.h>

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

uint16_t decode_symbol(node_huffman *root, uint8_t *buffer, int size,
                       int *current_byte, uint8_t *current_bit,
                       uint8_t last_byte_bits) {

  node_huffman *current = root;

  while (current->node_type == 0) {
    if (*current_byte == size - 1 && *current_bit >= last_byte_bits) {
      return 0;
    }

    if (*current_byte >= size) {
      return 0;
    }

    uint8_t bit = (buffer[*current_byte] >> (7 - *current_bit)) & 1;

    (*current_bit)++;

    if (*current_bit == 8) {
      *current_bit = 0;
      (*current_byte)++;
    }

    if (bit == 0) {
      current = current->left;
    } else {
      current = current->right;
    }
  }

  return current->pair;
}

void destroy_huffman_tree(node_huffman *root) {
  if (root == NULL)
    return;

  if (root->node_type == 0) {
    destroy_huffman_tree(root->left);
    destroy_huffman_tree(root->right);
  }
  free(root);
}
