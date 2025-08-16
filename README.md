# Linux Storage Daemon

A high-performance key-value storage daemon with Unix domain socket IPC, implementing a block-based storage engine with hybrid C/C++ architecture.

## Features

- **Block-based storage**: 64MB fixed-size storage files with 4KB blocks
- **Hybrid architecture**: Pure C core with modern C++ wrapper
- **Unix daemon**: Proper daemonization with signal handling
- **Thread-safe**: Mutex-protected operations for concurrent access
- **RAII design**: Automatic resource management in C++ layer
- **Unix domain sockets**: Fast local IPC communication

## Architecture

```
Client Application
       ↓
[C++ StorageEngine]  ← Modern C++ API (RAII, std::optional)
       ↓
[C storage functions] ← Core storage engine (PUT/GET/DELETE)
       ↓
[64MB block files]   ← Raw storage on disk
```

## Building

```bash
# Build all components
make all

# Run tests
make test-all

# Clean build artifacts
make clean
```

## Usage

### As a Library

```cpp
#include "include/server/StorageEngine.hpp"

storage::StorageEngine engine("data.db");
engine.initialize();

// Store data
engine.put("key", "value");

// Retrieve data
auto value = engine.getString("key");
if (value) {
    std::cout << "Value: " << *value << std::endl;
}
```

### As a Daemon (Coming Soon)

```bash
# Start daemon
./storage_daemon -f /path/to/storage.db

# Client operations
./storage_client put key value
./storage_client get key
./storage_client delete key
```

## Testing

```bash
# Run C tests
make test

# Run C++ tests
make test-cpp

# Run all tests
make test-all

# Docker-based Linux testing
./docker-test.sh
```

## Project Structure

```
├── include/core/       # C headers
│   ├── storage.h      # Storage engine API
│   └── daemon.h       # Daemon protocol
├── include/server/     # C++ headers
│   └── StorageEngine.hpp
├── src/core/          # C implementation
│   ├── storage.c      # Block storage engine
│   └── daemon.c       # Unix daemon
├── src/server/        # C++ implementation
│   └── StorageEngine.cpp
└── test_*.c/cpp       # Test suites
```

## Development Status

- ✅ Block-based storage engine
- ✅ C++ RAII wrapper
- ✅ Unix daemon structure
- 🚧 Client-server protocol
- 🚧 Multi-threading support
- 🚧 Client CLI tool

## License

MIT