#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <mutex>
#include "../core/storage.h"

namespace storage {

class StorageEngine {
private:
    std::string storage_file_;
    mutable std::mutex storage_mutex_;
    bool initialized_;

public:
    explicit StorageEngine(const std::string& storage_file);
    ~StorageEngine();
    
    // Disable copy constructor and assignment (singleton-like behavior)
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;
    
    // Move constructor and assignment
    StorageEngine(StorageEngine&& other) noexcept;
    StorageEngine& operator=(StorageEngine&& other) noexcept;
    
    // Core operations
    bool initialize();
    bool put(const std::string& key, const std::vector<uint8_t>& value);
    bool put(const std::string& key, const std::string& value);
    std::optional<std::vector<uint8_t>> get(const std::string& key);
    std::optional<std::string> getString(const std::string& key);
    bool remove(const std::string& key);
    
    // Status operations
    bool isInitialized() const;
    const std::string& getStorageFile() const;
    
    // Statistics (could be extended)
    struct Stats {
        size_t total_keys = 0;
        size_t total_size = 0;
        // TODO: Add more statistics from metadata
    };
    
    Stats getStats() const;

private:
    void cleanup();
};

} // namespace storage

#endif // STORAGE_ENGINE_HPP