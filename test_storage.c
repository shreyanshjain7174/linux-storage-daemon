#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/core/storage.h"

#define TEST_FILE "test_storage.db"

void print_test_result(const char* test_name, int passed) {
    if (passed) {
        printf("✓ %s passed\n", test_name);
    } else {
        printf("✗ %s failed\n", test_name);
    }
}

void test_basic_operations(void) {
    printf("\n=== Basic Storage Test ===\n");
    
    // Debug: Check struct sizes
    printf("DEBUG: sizeof(struct data_block) = %zu\n", sizeof(struct data_block));
    printf("DEBUG: sizeof(struct metadata_block) = %zu\n", sizeof(struct metadata_block));
    printf("DEBUG: BLOCK_SIZE = %d\n", BLOCK_SIZE);
    
    // Initialize storage
    int result = storage_init(TEST_FILE);
    print_test_result("Initialize storage", result == 0);
    
    if (result != 0) {
        printf("Cannot continue - storage init failed\n");
        return;
    }
    
    // Test simple PUT
    const char* key = "testkey";
    const char* value = "testvalue";
    result = storage_put(key, value, strlen(value) + 1);
    print_test_result("PUT operation", result == 0);
    
    // Test simple GET
    char buffer[100];
    size_t buffer_size = sizeof(buffer);
    result = storage_get(key, buffer, &buffer_size);
    int get_success = (result == 0 && strcmp(buffer, value) == 0);
    print_test_result("GET operation", get_success);
    
    if (!get_success && result == 0) {
        printf("  Expected: '%s', Got: '%s'\n", value, buffer);
    }
    
    // Test DELETE
    result = storage_delete(key);
    print_test_result("DELETE operation", result == 0);
    
    // Verify DELETE worked
    buffer_size = sizeof(buffer);
    result = storage_get(key, buffer, &buffer_size);
    print_test_result("Verify key deleted", result == -1);
    
    storage_cleanup();
}

int main(void) {
    printf("========================================\n");
    printf("    Storage Engine Test Suite\n");
    printf("========================================\n");
    
    // Run basic test only
    test_basic_operations();
    
    // Cleanup
    remove(TEST_FILE);
    
    printf("\n========================================\n");
    printf("    Basic test completed!\n");
    printf("========================================\n");
    
    return 0;
}
