#include <iostream>
#include <vector>
#include <cassert>
#include "include/server/StorageEngine.hpp"

void print_test_result(const std::string& test_name, bool passed) {
    if (passed) {
        std::cout << "✓ " << test_name << " passed\n";
    } else {
        std::cout << "✗ " << test_name << " failed\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    C++ Storage Engine Test Suite\n";
    std::cout << "========================================\n";
    
    // Create storage engine
    storage::StorageEngine engine("test_storage_cpp.db");
    
    // Test initialization
    bool init_result = engine.initialize();
    print_test_result("Storage Engine Initialization", init_result);
    
    if (!init_result) {
        std::cerr << "Cannot continue - storage init failed\n";
        return 1;
    }
    
    // Test string storage
    std::string test_key = "test_key";
    std::string test_value = "Hello, C++ Storage!";
    
    bool put_result = engine.put(test_key, test_value);
    print_test_result("PUT string operation", put_result);
    
    // Test string retrieval
    auto retrieved_value = engine.getString(test_key);
    bool get_result = retrieved_value.has_value() && 
                     retrieved_value.value() == test_value;
    print_test_result("GET string operation", get_result);
    
    if (!get_result && retrieved_value.has_value()) {
        std::cout << "  Expected: '" << test_value << "'\n";
        std::cout << "  Got: '" << retrieved_value.value() << "'\n";
    }
    
    // Test binary data storage
    std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
    std::string binary_key = "binary_key";
    
    bool binary_put = engine.put(binary_key, binary_data);
    print_test_result("PUT binary operation", binary_put);
    
    auto retrieved_binary = engine.get(binary_key);
    bool binary_get = retrieved_binary.has_value() && 
                     retrieved_binary.value() == binary_data;
    print_test_result("GET binary operation", binary_get);
    
    // Test deletion
    bool delete_result = engine.remove(test_key);
    print_test_result("DELETE operation", delete_result);
    
    // Verify deletion
    auto deleted_value = engine.getString(test_key);
    bool verify_delete = !deleted_value.has_value();
    print_test_result("Verify key deleted", verify_delete);
    
    // Test statistics
    auto stats = engine.getStats();
    std::cout << "\nStorage Statistics:\n";
    std::cout << "  Total keys: " << stats.total_keys << "\n";
    std::cout << "  Total size: " << stats.total_size << " bytes\n";
    
    std::cout << "\n========================================\n";
    std::cout << "    C++ test completed!\n";
    std::cout << "========================================\n";
    
    return 0;
}