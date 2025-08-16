#include "../../include/server/StorageEngine.hpp"
#include <iostream>
#include <cstring>

namespace storage {

StorageEngine::StorageEngine(const std::string& storage_file)
    : storage_file_(storage_file), initialized_(false) {
}

StorageEngine::~StorageEngine() {
    cleanup();
}

StorageEngine::StorageEngine(StorageEngine&& other) noexcept
    : storage_file_(std::move(other.storage_file_)),
      initialized_(other.initialized_) {
    other.initialized_ = false;
}

StorageEngine& StorageEngine::operator=(StorageEngine&& other) noexcept {
    if (this != &other) {
        cleanup();
        storage_file_ = std::move(other.storage_file_);
        initialized_ = other.initialized_;
        other.initialized_ = false;
    }
    return *this;
}

bool StorageEngine::initialize() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (initialized_) {
        return true;
    }
    
    int result = storage_init(storage_file_.c_str());
    if (result == 0) {
        initialized_ = true;
        return true;
    }
    
    return false;
}

bool StorageEngine::put(const std::string& key, const std::vector<uint8_t>& value) {
    if (!initialized_) {
        return false;
    }
    
    if (key.empty() || key.size() >= MAX_KEY_SIZE) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    int result = storage_put(key.c_str(), 
                              reinterpret_cast<const char*>(value.data()), 
                              value.size());
    
    return result == 0;
}

bool StorageEngine::put(const std::string& key, const std::string& value) {
    std::vector<uint8_t> data(value.begin(), value.end());
    data.push_back('\0'); // Null terminate for string storage
    return put(key, data);
}

std::optional<std::vector<uint8_t>> StorageEngine::get(const std::string& key) {
    if (!initialized_) {
        return std::nullopt;
    }
    
    if (key.empty() || key.size() >= MAX_KEY_SIZE) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // First, try to get the size
    size_t buffer_size = 0;
    int result = storage_get(key.c_str(), nullptr, &buffer_size);
    
    if (result != 0) {
        return std::nullopt;
    }
    
    // Allocate buffer and get the actual data
    std::vector<uint8_t> buffer(buffer_size);
    result = storage_get(key.c_str(), 
                          reinterpret_cast<char*>(buffer.data()), 
                          &buffer_size);
    
    if (result == 0) {
        buffer.resize(buffer_size); // Adjust to actual size returned
        return buffer;
    }
    
    return std::nullopt;
}

std::optional<std::string> StorageEngine::getString(const std::string& key) {
    auto data = get(key);
    if (!data) {
        return std::nullopt;
    }
    
    // Remove null terminator if present
    if (!data->empty() && data->back() == '\0') {
        data->pop_back();
    }
    
    return std::string(data->begin(), data->end());
}

bool StorageEngine::remove(const std::string& key) {
    if (!initialized_) {
        return false;
    }
    
    if (key.empty() || key.size() >= MAX_KEY_SIZE) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    int result = storage_delete(key.c_str());
    return result == 0;
}

bool StorageEngine::isInitialized() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return initialized_;
}

const std::string& StorageEngine::getStorageFile() const {
    return storage_file_;
}

StorageEngine::Stats StorageEngine::getStats() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // TODO: Implement actual statistics gathering
    // For now, return empty stats
    Stats stats;
    return stats;
}

void StorageEngine::cleanup() {
    if (initialized_) {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        storage_cleanup();
        initialized_ = false;
    }
}

} // namespace storage