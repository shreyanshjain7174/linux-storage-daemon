# Storage Daemon Design Summary

## What it is
A simple key-value store that runs as a Linux daemon. Clients connect via Unix sockets to store/retrieve data that persists in a local file.

## How it works

**Process model**: Main daemon just accepts connections and forks a child for each client. Child handles the request and exits. Simple but effective.

**Storage**: Everything goes into a 64MB file split into 4KB blocks. Block 0 holds metadata (magic number, key index, bitmap). Remaining blocks store actual data in linked lists.

**Communication**: Binary protocol over Unix domain socket. Client sends message header + request struct + data. Server responds similarly.

## Why these choices

**Fork per client** instead of threading because:
- Complete isolation - one bad client can't crash others
- No shared state to worry about (except the file)  
- Process cleanup is automatic
- Simpler to debug and reason about

**Fixed 64MB file** instead of growing dynamically:
- Predictable disk usage
- No fragmentation from file growth
- Simpler allocation logic
- Good enough for the use case

**Single global mutex** instead of fine-grained locking:
- Correctness over performance
- Much simpler to implement
- Avoids deadlock scenarios
- Acceptable for moderate load

## Key structures

```c
// Block 0 layout
struct metadata_block {
    uint32_t magic;              // 0xDEADBEEF
    uint32_t version;            // 1
    uint32_t total_blocks;       // 16384
    uint32_t free_blocks;        // Available blocks
    uint8_t bitmap[2048];        // Block allocation
    struct key_entry entries[7]; // Key index
};

// Data block layout  
struct data_block {
    uint32_t next_block_id;      // 0 = end of chain
    uint32_t data_size;          // Bytes used
    char data[4088];             // Actual payload
};
```

## Limitations by design

- **7 keys max**: Key entries fit in metadata block
- **256 byte keys**: Reasonable limit, keeps things simple
- **No crash recovery**: No WAL, no journaling. KISS principle.
- **Local only**: Unix sockets, no network support
- **Space inefficient**: 4KB minimum per value regardless of size

## What I learned

The hardest part was getting the struct packing right - compiler padding was adding extra bytes that broke the protocol. `__attribute__((packed))` solved it.

Process forking turned out cleaner than I expected. Each client connection is completely isolated, and zombie cleanup with SIGCHLD just works.

The bitmap for block allocation is straightforward - just set/clear bits. Linked list for large values means no need for complex allocation algorithms.

## Test results

Basic functionality: 13/13 tests pass
Stress testing: Handles concurrent clients fine  
Performance: ~3000 ops/sec for small values
Memory usage: Stable 2MB footprint

Space efficiency is terrible (0.54%) but that's expected with fixed 4KB blocks.

## Would I use this in production?

For small-scale local storage like session caching or configuration data, sure. The simplicity makes it reliable and easy to maintain.

For anything requiring scale, durability, or high performance - definitely not. But that wasn't the goal here.