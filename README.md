# RLE + Huffman compressor

Lossless data compressor from scratch in C.

- **Implemented**: RLE, min-heap, Huffman tree, XOR protection (uses a hardcoded key = 0xAB), encoding/decoding (RLE+Huffman), CLI, size verification
- **Note**: The compressor may crash on large files with high entropy, probably due to a stack overflow in recursive functions

# Build and run 
```shell
git clone https://github.com/im-yasei/rle-huffman-compressor.git
cd rle-huffman-compressor

# Build
make

# Run (to compress your file)
./compressor -c your_file.extension compressed_file.rlhfmn

# Run (to decompress your file)
./compressor -d compressed_file.rlhfmn your_new_file.extension
```

