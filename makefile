CC = gcc
OBJS = obj/compressor.o obj/minheap.o obj/huffman_tree.o obj/huffman_code.o obj/utils.o


all: compressor

compressor: $(OBJS)
	$(CC) $(OBJS) -o compressor

obj/compressor.o: src/compressor.c include/compressor.h | obj
	$(CC) -c src/compressor.c -o obj/compressor.o

obj/minheap.o: src/minheap.c include/compressor.h | obj
	$(CC) -c src/minheap.c -o obj/minheap.o

obj/huffman_tree.o: src/huffman_tree.c include/compressor.h | obj
	$(CC) -c src/huffman_tree.c -o obj/huffman_tree.o

obj/huffman_code.o: src/huffman_code.c include/compressor.h | obj
	$(CC) -c src/huffman_code.c -o obj/huffman_code.o

obj/utils.o: src/utils.c include/compressor.h | obj
	$(CC) -c src/utils.c -o obj/utils.o

obj:
	mkdir -p obj

clean:
	rm -rf obj compressor

.PHONY: all clean
