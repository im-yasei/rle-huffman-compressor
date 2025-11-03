#include "../include/compressor.h"
#include <stdlib.h>

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
  if (heap->size >= heap->capacity) {
    heap->capacity *= 2;
    heap->nodes = realloc(heap->nodes, heap->capacity * sizeof(node_huffman *));
  }

  heap->nodes[heap->size] = node;
  heap->size++;

  int i = heap->size - 1;

  while (i > 0) {

    if (heap->nodes[parent(i)]->frequency > heap->nodes[i]->frequency) {
      swap_nodes(&heap->nodes[i], &heap->nodes[parent(i)]);
      i = parent(i);
    } else if (heap->nodes[parent(i)]->frequency == heap->nodes[i]->frequency) {
      if (heap->nodes[parent(i)]->pair > heap->nodes[i]->pair) {
        swap_nodes(&heap->nodes[i], &heap->nodes[parent(i)]);
        i = parent(i);
      } else if (heap->nodes[i]->node_type == 0 &&
                 heap->nodes[parent(i)]->node_type == 1) {
        swap_nodes(&heap->nodes[i], &heap->nodes[parent(i)]);
        i = parent(i);
      } else {
        break;
      }
    } else {
      break;
    }
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
