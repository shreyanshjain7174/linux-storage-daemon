# Linux Storage Daemon

Simple key-value storage daemon for Linux with block-based persistence.

## Quick Start

```bash
# Build
make all

# Start daemon  
./bin/storage_daemon ./storage.db

# Use client
./bin/storage_client put mykey "hello world"
./bin/storage_client get mykey
./bin/storage_client delete mykey
```

## Architecture

**Core Design**: Process-forking daemon with Unix domain sockets
- Each client connection spawns a child process for isolation
- Block-based storage with 64MB fixed files (4KB blocks)
- 7 key maximum, 256 byte key limit
- Mutex-protected storage operations

**Files**:
- `src/core/` - Storage engine and daemon logic (C)
- `src/client/` - Client library and CLI (C) 
- `include/` - Headers
- `tests/` - Test suite

## Storage Layout

```
Block 0: Metadata (magic, version, bitmap, key index)
Block 1-16383: Data blocks (linked list structure)
```

Each key entry stores: key name (256B), first block ID, value size, valid flag.

## Process Model

1. Main daemon accepts connections on `/tmp/storage_daemon.sock`
2. Fork child process for each client
3. Child handles PUT/GET/DELETE, exits when done
4. Shared storage file protected by mutex

## Testing

```bash
# Run all tests
make test-all

# Individual test suites
./tests/test.sh          # Basic functionality
./tests/stress_test.sh   # Concurrent operations  
./tests/performance_test.sh  # Latency/throughput

# Docker testing (Linux)
./run_tests.sh
```

## Limitations

- Max 7 keys total
- 64MB storage file size
- No crash recovery (no WAL)
- Local access only
- Single storage file

## Building

Requires GCC and POSIX headers. No external dependencies.

```bash
make all        # Build daemon + client
make clean      # Clean build files
```

Tested on Linux containers, builds on macOS for development.