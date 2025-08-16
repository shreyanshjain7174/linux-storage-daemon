#ifndef CORE_STORAGE_H
#define CORE_STORAGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Core storage data structures (remain in C)
#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 16384  // 64MB / 4KB
#define MAX_KEY_SIZE 256
#define MAX_KEYS 7          // Limited by Block 0 space

struct key_entry {
    char key[MAX_KEY_SIZE];
    uint32_t first_block_id;
    uint32_t value_size;
    uint8_t is_valid;
} __attribute__((packed));

struct metadata_block {
    uint32_t magic;         // 0xDEADBEEF
    uint32_t version;       // 1
    uint32_t total_blocks;  // 16384
    uint32_t free_blocks;   // Current free blocks
    uint8_t bitmap[2048];   // 16384 bits for free blocks
    struct key_entry entries[MAX_KEYS];
    uint8_t padding[177];   // Fill to 4096 bytes
} __attribute__((packed));

struct data_block {
    uint32_t next_block_id; // 0 = last block
    uint32_t data_size;     // Bytes used in this block
    uint8_t data[4088];     // Actual data
} __attribute__((packed));

// Core C API - clean interface for C++ wrapping
int storage_init(const char* filename);
int storage_put(const char* key, const char* value, size_t value_size);
int storage_get(const char* key, char* value, size_t* value_size);
int storage_delete(const char* key);
void storage_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // CORE_STORAGE_H
