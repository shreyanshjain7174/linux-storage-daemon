#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../include/core/storage.h"

// Global storage file descriptor
static int storage_fd = -1;
static char* storage_filename = NULL;

// Helper function to find a free block in the bitmap
static int find_free_block(struct metadata_block *meta) {
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        if (!(meta->bitmap[byte_index] & (1 << bit_index))) {
            return i;  // Found a free block
        }
    }
    return -1;  // No free blocks
}

// Helper function to mark a block as used
static void mark_block_used(struct metadata_block *meta, int block_id) {
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    meta->bitmap[byte_index] |= (1 << bit_index);
    meta->free_blocks--;
}

// Helper function to mark a block as free
static void mark_block_free(struct metadata_block *meta, int block_id) {
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    meta->bitmap[byte_index] &= ~(1 << bit_index);
    meta->free_blocks++;
}

// Helper function to read metadata from Block 0
static int read_metadata(struct metadata_block *meta) {
    if (storage_fd < 0) return -1;
    
    if (lseek(storage_fd, 0, SEEK_SET) == -1) {
        return -1;
    }
    
    if (read(storage_fd, meta, sizeof(*meta)) != sizeof(*meta)) {
        return -1;
    }
    
    return 0;
}

// Helper function to write metadata to Block 0
static int write_metadata(struct metadata_block *meta) {
    if (storage_fd < 0) return -1;
    
    if (lseek(storage_fd, 0, SEEK_SET) == -1) {
        return -1;
    }
    
    if (write(storage_fd, meta, sizeof(*meta)) != sizeof(*meta)) {
        return -1;
    }
    
    return 0;
}

int storage_init(const char *filename) {
    // Save filename for later use
    if (storage_filename) {
        free(storage_filename);
    }
    storage_filename = strdup(filename);
    
    // Try to open existing file
    storage_fd = open(filename, O_RDWR);
    
    if (storage_fd == -1) {
        // File doesn't exist, create new one
        storage_fd = open(filename, O_CREAT | O_RDWR, 0644);
        if (storage_fd == -1) {
            return -1;
        }
        
        // Create 64MB file
        if (lseek(storage_fd, 64 * 1024 * 1024 - 1, SEEK_SET) == -1) {
            close(storage_fd);
            storage_fd = -1;
            return -1;
        }
        write(storage_fd, "", 1);
        
        // Go back to start and initialize metadata
        lseek(storage_fd, 0, SEEK_SET);
        
        struct metadata_block meta;
        memset(&meta, 0, sizeof(meta));
        meta.magic = 0xDEADBEEF;
        meta.version = 1;
        meta.total_blocks = TOTAL_BLOCKS;
        meta.free_blocks = TOTAL_BLOCKS - 1;  // Block 0 is used for metadata
        
        // Initialize bitmap - all blocks free except block 0
        memset(meta.bitmap, 0, sizeof(meta.bitmap));
        meta.bitmap[0] = 0x01;  // Mark Block 0 as used
        
        // Initialize all key entries as invalid
        for (int i = 0; i < MAX_KEYS; i++) {
            meta.entries[i].is_valid = 0;
        }
        
        // Write metadata to Block 0
        if (write(storage_fd, &meta, sizeof(meta)) != sizeof(meta)) {
            close(storage_fd);
            storage_fd = -1;
            return -1;
        }
    } else {
        // File exists, validate it
        struct metadata_block meta;
        if (read_metadata(&meta) != 0) {
            close(storage_fd);
            storage_fd = -1;
            return -1;
        }
        
        // Validate metadata
        if (meta.magic != 0xDEADBEEF || meta.version != 1) {
            close(storage_fd);
            storage_fd = -1;
            return -1;
        }
    }
    
    return 0;  // Success
}

int storage_put(const char* key, const char* value, size_t value_size) {
    if (storage_fd < 0 || !key || !value) {
        return -1;
    }
    
    // Check key length
    if (strlen(key) >= MAX_KEY_SIZE) {
        return -1;
    }
    
    // Read metadata
    struct metadata_block meta;
    if (read_metadata(&meta) != 0) {
        return -1;
    }
    
    // Find if key already exists or find empty slot
    int empty_slot = -1;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (meta.entries[i].is_valid) {
            if (strcmp(meta.entries[i].key, key) == 0) {
                // Key exists, need to free old blocks first
                // TODO: Implement freeing old blocks
                empty_slot = i;
                break;
            }
        } else if (empty_slot == -1) {
            empty_slot = i;
        }
    }
    
    if (empty_slot == -1) {
        return -1;  // No space for new key
    }
    
    // Calculate blocks needed based on data area size, not full struct size
    int blocks_needed = (value_size + sizeof(((struct data_block*)0)->data) - 1) / sizeof(((struct data_block*)0)->data);
    if ((uint32_t)blocks_needed > meta.free_blocks) {
        return -1;  // Not enough space
    }
    
    // Allocate blocks and write data
    int first_block = -1;
    int prev_block = -1;
    size_t bytes_written = 0;
    
    while (bytes_written < value_size) {
        int block_id = find_free_block(&meta);
        if (block_id == -1) {
            return -1;  // No free blocks (shouldn't happen)
        }
        
        mark_block_used(&meta, block_id);
        
        if (first_block == -1) {
            first_block = block_id;
        }
        
        // Prepare data block
        struct data_block block;
        memset(&block, 0, sizeof(block));
        
        size_t to_write = value_size - bytes_written;
        if (to_write > sizeof(block.data)) {
            to_write = sizeof(block.data);
        }
        
        memcpy(block.data, value + bytes_written, to_write);
        block.data_size = to_write;
        block.next_block_id = 0;  // Will be updated if there's a next block
        
        // Write block to file - must write full BLOCK_SIZE to maintain alignment
        printf("DEBUG PUT: Writing block %d at offset %ld\n", block_id, (long)(block_id * BLOCK_SIZE));
        printf("DEBUG PUT: Block data_size: %u, next_block_id: %u\n", block.data_size, block.next_block_id);
        printf("DEBUG PUT: Writing data: '%.10s'\n", block.data);
        
        if (lseek(storage_fd, block_id * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("DEBUG PUT: lseek failed for block %d\n", block_id);
            return -1;
        }
        
        // Create a full block buffer and zero-pad it
        char block_buffer[BLOCK_SIZE];
        memset(block_buffer, 0, BLOCK_SIZE);
        memcpy(block_buffer, &block, sizeof(block));
        
        if (write(storage_fd, block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
            printf("DEBUG PUT: write failed for block %d\n", block_id);
            return -1;
        }
        
        // Update previous block's next pointer
        if (prev_block != -1) {
            if (lseek(storage_fd, prev_block * BLOCK_SIZE, SEEK_SET) == -1) {
                return -1;
            }
            
            // Read the full block to preserve data alignment
            char prev_block_buffer[BLOCK_SIZE];
            if (read(storage_fd, prev_block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
                return -1;
            }
            
            // Update the next_block_id in the data structure
            struct data_block *prev = (struct data_block*)prev_block_buffer;
            prev->next_block_id = block_id;
            
            // Write back the full block
            lseek(storage_fd, prev_block * BLOCK_SIZE, SEEK_SET);
            write(storage_fd, prev_block_buffer, BLOCK_SIZE);
        }
        
        bytes_written += to_write;
        prev_block = block_id;
    }
    
    // Update key entry
    strcpy(meta.entries[empty_slot].key, key);
    meta.entries[empty_slot].first_block_id = first_block;
    meta.entries[empty_slot].value_size = value_size;
    meta.entries[empty_slot].is_valid = 1;
    
    // Write updated metadata
    if (write_metadata(&meta) != 0) {
        return -1;
    }
    
    return 0;  // Success
}

int storage_get(const char* key, char* value, size_t* value_size) {
    if (storage_fd < 0 || !key || !value_size) {
        printf("DEBUG: storage_get - Invalid parameters\n");
        return -1;
    }
    
    printf("DEBUG: storage_get - Looking for key: '%s'\n", key);
    
    // Read metadata
    struct metadata_block meta;
    if (read_metadata(&meta) != 0) {
        printf("DEBUG: storage_get - Failed to read metadata\n");
        return -1;
    }
    
    // Find key
    int found = -1;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (meta.entries[i].is_valid) {
            printf("DEBUG: Found valid key[%d]: '%s'\n", i, meta.entries[i].key);
            if (strcmp(meta.entries[i].key, key) == 0) {
                found = i;
                printf("DEBUG: Key match found at index %d\n", i);
                break;
            }
        }
    }
    
    if (found == -1) {
        printf("DEBUG: storage_get - Key not found\n");
        return -1;  // Key not found
    }
    
    printf("DEBUG: Key found - first_block_id: %u, value_size: %u\n", 
           meta.entries[found].first_block_id, meta.entries[found].value_size);
    
    // If value is null, caller just wants the size
    if (value == NULL) {
        *value_size = meta.entries[found].value_size;
        return 0;  // Success - size returned
    }
    
    // Check buffer size
    if (*value_size < meta.entries[found].value_size) {
        *value_size = meta.entries[found].value_size;
        return -1;  // Buffer too small
    }
    
    // Read data blocks
    int block_id = meta.entries[found].first_block_id;
    size_t bytes_read = 0;
    
    printf("DEBUG: Starting to read data blocks, first block_id: %d\n", block_id);
    
    while (block_id != 0 && bytes_read < meta.entries[found].value_size) {
        printf("DEBUG: Reading block %d at offset %ld\n", block_id, (long)(block_id * BLOCK_SIZE));
        
        // Read full block to maintain alignment
        if (lseek(storage_fd, block_id * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("DEBUG: lseek failed for block %d\n", block_id);
            return -1;
        }
        
        char block_buffer[BLOCK_SIZE];
        ssize_t bytes_read_from_file = read(storage_fd, block_buffer, BLOCK_SIZE);
        if (bytes_read_from_file != BLOCK_SIZE) {
            printf("DEBUG: read failed - expected %d bytes, got %zd\n", BLOCK_SIZE, bytes_read_from_file);
            return -1;
        }
        
        // Extract data block structure
        struct data_block *block = (struct data_block*)block_buffer;
        
        printf("DEBUG: Block data_size: %u, next_block_id: %u\n", block->data_size, block->next_block_id);
        printf("DEBUG: First few bytes of data: '%.10s'\n", block->data);
        
        // Calculate how much data to copy from this block
        size_t remaining = meta.entries[found].value_size - bytes_read;
        size_t to_copy = (remaining < block->data_size) ? remaining : block->data_size;
        
        printf("DEBUG: Remaining: %zu, block data_size: %u, to_copy: %zu\n", remaining, block->data_size, to_copy);
        
        // Copy data
        memcpy(value + bytes_read, block->data, to_copy);
        bytes_read += to_copy;
        
        // Move to next block
        block_id = block->next_block_id;
    }
    
    *value_size = bytes_read;
    return 0;  // Success
}

int storage_delete(const char* key) {
    if (storage_fd < 0 || !key) {
        return -1;
    }
    
    // Read metadata
    struct metadata_block meta;
    if (read_metadata(&meta) != 0) {
        return -1;
    }
    
    // Find key
    int found = -1;
    for (int i = 0; i < MAX_KEYS; i++) {
        if (meta.entries[i].is_valid && strcmp(meta.entries[i].key, key) == 0) {
            found = i;
            break;
        }
    }
    
    if (found == -1) {
        return -1;  // Key not found
    }
    
    // Free all blocks used by this key
    int block_id = meta.entries[found].first_block_id;
    
    while (block_id != 0) {
        // Read full block to get next block
        if (lseek(storage_fd, block_id * BLOCK_SIZE, SEEK_SET) == -1) {
            return -1;
        }
        
        char block_buffer[BLOCK_SIZE];
        if (read(storage_fd, block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
            return -1;
        }
        
        struct data_block *block = (struct data_block*)block_buffer;
        
        // Store next block ID before marking current block as free
        int next_block = block->next_block_id;
        
        // Mark block as free
        mark_block_free(&meta, block_id);
        
        // Move to next block
        block_id = next_block;
    }
    
    // Mark key entry as invalid
    meta.entries[found].is_valid = 0;
    memset(meta.entries[found].key, 0, MAX_KEY_SIZE);
    meta.entries[found].first_block_id = 0;
    meta.entries[found].value_size = 0;
    
    // Write updated metadata
    if (write_metadata(&meta) != 0) {
        return -1;
    }
    
    return 0;  // Success
}

void storage_cleanup(void) {
    if (storage_fd >= 0) {
        close(storage_fd);
        storage_fd = -1;
    }
    
    if (storage_filename) {
        free(storage_filename);
        storage_filename = NULL;
    }
}
