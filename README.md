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

## Overview of Design and Key Components

### Architecture
**Core Design**: Process-forking daemon with Unix domain sockets
- **Daemon Process**: True Linux daemon using double-fork technique with setsid()
- **IPC Method**: Unix domain sockets at `/tmp/storage_daemon.sock`
- **Storage Engine**: Custom block-based storage with 64MB fixed files
- **Concurrency**: Process isolation through fork() - each client gets own child process

### Key Components
1. **Core Storage Engine** (`src/core/storage.c`)
   - Block-based key-value store with 4KB blocks
   - Free block management using bitmap
   - Metadata indexing in Block 0

2. **Daemon Process** (`src/core/daemon.c`)
   - Proper daemonization (fork, setsid, signal handling)
   - Unix domain socket server
   - Message protocol handling (PUT/GET/DELETE)

3. **Client Library** (`src/client/storage_client.c`)
   - Socket communication with protocol messages
   - Synchronous request-response operations
   - Error handling and connection management

## Data Layout for Storage Backend

### Block Structure
```
File Layout (64MB total):
┌─────────────────────────────────────────────────────────────┐
│ Block 0: Metadata Block (4096 bytes)                       │
├─────────────────────────────────────────────────────────────┤
│ Block 1-16383: Data Blocks (4096 bytes each)              │
└─────────────────────────────────────────────────────────────┘

Block 0 (Metadata):
├── Magic Number (4 bytes): 0xDEADBEEF
├── Version (4 bytes): 1
├── Total Blocks (4 bytes): 16384
├── Free Blocks Count (4 bytes)
├── Bitmap (2048 bytes): Tracks block allocation
├── Key Entries[7]: Each entry (265 bytes):
│   ├── Key (256 bytes): Null-terminated string
│   ├── First Block ID (4 bytes): Start of value chain
│   ├── Value Size (4 bytes): Total value length
│   └── Valid Flag (1 byte): Entry active flag
└── Padding (177 bytes)

Data Block (1-16383):
├── Next Block ID (4 bytes): Link to next block (0 = end)
├── Data Size (4 bytes): Actual data in this block
└── Data (4088 bytes): Value data payload
```

### Storage Characteristics
- **File Size**: Fixed 64MB (16384 blocks × 4KB)
- **Max Keys**: 7 (limited by metadata block capacity)
- **Max Key Size**: 255 bytes (null-terminated)
- **Max Value Size**: ~67MB (4088 bytes × 16383 blocks)
- **Block Allocation**: Linked-list structure for large values

## Concurrency Model

### Process-Based Isolation
- **Main Process**: Accepts connections, never handles client data
- **Child Processes**: Fork per client connection, handle single request, exit
- **Process Benefits**: Complete isolation, automatic cleanup, crash containment

### Synchronization
- **Mutex Protection**: `pthread_mutex_t storage_mutex` protects all storage operations
- **File Locking**: Ensures atomic access to storage file across processes
- **Signal Handling**: 
  - SIGTERM/SIGINT: Graceful shutdown
  - SIGCHLD: Child process cleanup
  - SIGPIPE: Ignored (broken client connections)

### Request Flow
1. Client connects → daemon accept() returns
2. Daemon forks child process
3. Child reads message, acquires storage mutex
4. Child performs storage operation (PUT/GET/DELETE)
5. Child releases mutex, sends response
6. Child process exits, parent continues accepting

## Design Trade-offs and Assumptions

### Trade-offs Made
1. **Simplicity over Performance**: 
   - Fixed 64MB file vs dynamic growth
   - Single mutex vs fine-grained locking
   - Process forking vs threading

2. **Reliability over Efficiency**:
   - Process isolation vs shared memory
   - Synchronous I/O vs async/mmap
   - Fixed block size vs variable allocation

3. **Development Speed over Optimization**:
   - Linear key search vs hash table/B-tree
   - No compression vs space optimization
   - Simple protocol vs advanced features

### Key Assumptions
- **Usage Pattern**: Small number of keys (≤7), moderate value sizes
- **Client Behavior**: Short-lived connections, infrequent access
- **Environment**: Local access only, trusted users
- **Data**: Keys are ASCII strings, values can be binary
- **Storage**: Sufficient disk space for 64MB file
- **System**: POSIX-compliant Linux/Unix environment

## Known Limitations

### Functional Limits
- **Key Capacity**: Maximum 7 concurrent keys
- **Key Size**: 255 bytes (null-terminated strings)
- **File Size**: Fixed 64MB storage allocation
- **Concurrency**: One storage operation at a time (mutex bottleneck)

### Reliability Issues
- **No Crash Recovery**: No write-ahead log or journaling
- **No ACID Guarantees**: Partial writes possible during crashes
- **No Backup/Replication**: Single point of failure
- **Corruption Detection**: Limited to magic number validation

### Performance Limitations
- **Space Efficiency**: 0.54% efficiency for small values (4KB minimum per value)
- **Block Granularity**: No sub-block allocation
- **Sequential Access**: Linear key search in metadata
- **Process Overhead**: Fork cost per client connection

### Security/Access
- **No Authentication**: Any local user can connect
- **No Encryption**: Data stored in plaintext
- **Local Only**: Unix domain socket limits to same machine
- **No Access Control**: No user/permission model

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

## Building

Requires GCC and POSIX headers. No external dependencies.

```bash
make all        # Build daemon + client
make clean      # Clean build files
```

Tested on Linux containers, builds on macOS for development.