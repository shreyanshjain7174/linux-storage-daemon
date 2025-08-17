#!/bin/bash

# Performance Metrics Test
# Measures throughput, latency, and resource usage

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

DAEMON_BIN="./build/storage_daemon"
CLIENT_BIN="./build/storage_client"
STORAGE_FILE="/tmp/perf_storage.db"
SOCKET_PATH="/tmp/storage_daemon.sock"

# Test parameters
WARMUP_OPS=100
TEST_DURATION=10  # seconds
VALUE_SIZES=(64 256 1024 4096 16384)

# Clean up function
cleanup() {
    pkill -f storage_daemon 2>/dev/null || true
    rm -f $STORAGE_FILE $SOCKET_PATH
    rm -f /tmp/perf_*.tmp
}

trap cleanup EXIT

# Start daemon
start_daemon() {
    $DAEMON_BIN $STORAGE_FILE &
    DAEMON_PID=$!
    sleep 2
    
    if ! kill -0 $DAEMON_PID 2>/dev/null; then
        echo -e "${RED}Failed to start daemon${NC}"
        exit 1
    fi
}

# Generate value of specific size
generate_value() {
    local size=$1
    head -c $size /dev/urandom | base64 | head -c $size
}

# Warm up the daemon
warmup() {
    echo -n "Warming up daemon... "
    for i in $(seq 1 $WARMUP_OPS); do
        value=$(generate_value 256)
        $CLIENT_BIN put "warmup_$i" "$value" > /dev/null 2>&1
    done
    echo -e "${GREEN}DONE${NC}"
}

# Measure throughput
measure_throughput() {
    local operation=$1
    local value_size=$2
    local duration=$3
    
    local count=0
    local value=$(generate_value $value_size)
    local end_time=$(($(date +%s) + duration))
    
    while [ $(date +%s) -lt $end_time ]; do
        case $operation in
            "PUT")
                $CLIENT_BIN put "perf_key_$count" "$value" > /dev/null 2>&1
                ;;
            "GET")
                $CLIENT_BIN get "perf_key_$((count % 100))" > /dev/null 2>&1
                ;;
            "DELETE")
                $CLIENT_BIN delete "perf_key_$count" > /dev/null 2>&1
                ;;
        esac
        count=$((count + 1))
    done
    
    echo $count
}

# Measure latency
measure_latency() {
    local operation=$1
    local value_size=$2
    local samples=$3
    
    local total_time=0
    local min_time=999999999
    local max_time=0
    local value=$(generate_value $value_size)
    
    for i in $(seq 1 $samples); do
        start_time=$(date +%s%N)
        
        case $operation in
            "PUT")
                $CLIENT_BIN put "latency_key_$i" "$value" > /dev/null 2>&1
                ;;
            "GET")
                $CLIENT_BIN get "latency_key_$((i % 10 + 1))" > /dev/null 2>&1
                ;;
            "DELETE")
                $CLIENT_BIN delete "latency_key_$i" > /dev/null 2>&1
                ;;
        esac
        
        end_time=$(date +%s%N)
        elapsed=$((end_time - start_time))
        
        total_time=$((total_time + elapsed))
        
        if [ $elapsed -lt $min_time ]; then
            min_time=$elapsed
        fi
        
        if [ $elapsed -gt $max_time ]; then
            max_time=$elapsed
        fi
    done
    
    avg_time=$((total_time / samples / 1000000))  # Convert to ms
    min_time=$((min_time / 1000000))
    max_time=$((max_time / 1000000))
    
    echo "$avg_time $min_time $max_time"
}

# Get memory usage of daemon
get_memory_usage() {
    if [ -n "$DAEMON_PID" ] && kill -0 $DAEMON_PID 2>/dev/null; then
        ps -o rss= -p $DAEMON_PID | awk '{print $1/1024 " MB"}'
    else
        echo "N/A"
    fi
}

# Print header
print_header() {
    echo ""
    echo "========================================="
    echo "     PERFORMANCE METRICS REPORT"
    echo "========================================="
    echo ""
    echo -e "${BLUE}Test Configuration:${NC}"
    echo "  - Test Duration: ${TEST_DURATION}s"
    echo "  - Warmup Operations: ${WARMUP_OPS}"
    echo "  - Value Sizes: ${VALUE_SIZES[@]} bytes"
    echo ""
}

# Print results table
print_results_table() {
    local operation=$1
    shift
    local results=("$@")
    
    echo ""
    echo -e "${YELLOW}$operation Performance:${NC}"
    echo "┌──────────┬────────────┬──────────────────────────────┬─────────────┐"
    echo "│ Size (B) │ Throughput │ Latency (ms)                │ Memory      │"
    echo "│          │ (ops/sec)  │ Avg    Min    Max           │ Usage       │"
    echo "├──────────┼────────────┼──────────────────────────────┼─────────────┤"
    
    for result in "${results[@]}"; do
        echo "$result"
    done
    
    echo "└──────────┴────────────┴──────────────────────────────┴─────────────┘"
}

# Build the project
echo "Building project..."
make clean && make all > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Clean up and start
cleanup
start_daemon

print_header

# Warm up
warmup
echo ""

# Test each operation
for op in "PUT" "GET" "DELETE"; do
    results=()
    
    for size in "${VALUE_SIZES[@]}"; do
        # Prepare data for GET operations
        if [ "$op" = "GET" ]; then
            for i in $(seq 0 99); do
                value=$(generate_value $size)
                $CLIENT_BIN put "perf_key_$i" "$value" > /dev/null 2>&1
            done
        fi
        
        # Measure throughput
        ops=$(measure_throughput "$op" "$size" "$TEST_DURATION")
        throughput=$((ops / TEST_DURATION))
        
        # Measure latency
        latency_data=$(measure_latency "$op" "$size" 100)
        read avg_lat min_lat max_lat <<< "$latency_data"
        
        # Get memory usage
        mem_usage=$(get_memory_usage)
        
        # Format result
        result=$(printf "│ %-8d │ %-10d │ %-6d %-6d %-13d │ %-11s │" \
                 "$size" "$throughput" "$avg_lat" "$min_lat" "$max_lat" "$mem_usage")
        results+=("$result")
    done
    
    print_results_table "$op" "${results[@]}"
done

# Storage efficiency test
echo ""
echo -e "${YELLOW}Storage Efficiency:${NC}"
echo "--------------------------------"

# Add known data
total_key_size=0
total_value_size=0
num_entries=1000

for i in $(seq 1 $num_entries); do
    key="efficiency_test_key_$i"
    value=$(generate_value 100)
    
    $CLIENT_BIN put "$key" "$value" > /dev/null 2>&1
    
    total_key_size=$((total_key_size + ${#key}))
    total_value_size=$((total_value_size + 100))
done

# Check file size
file_size=$(stat -f%z "$STORAGE_FILE" 2>/dev/null || stat -c%s "$STORAGE_FILE" 2>/dev/null)
data_size=$((total_key_size + total_value_size))
overhead=$((file_size - data_size))
efficiency=$(echo "scale=2; $data_size * 100 / $file_size" | bc 2>/dev/null || echo "N/A")

echo "  - Number of entries: $num_entries"
echo "  - Total data size: $((data_size / 1024)) KB"
echo "  - Storage file size: $((file_size / 1024)) KB"
echo "  - Storage overhead: $((overhead / 1024)) KB"
echo "  - Storage efficiency: ${efficiency}%"
echo ""

# System resource usage
echo -e "${YELLOW}System Resources:${NC}"
echo "--------------------------------"
echo "  - Daemon PID: $DAEMON_PID"
echo "  - Memory usage: $(get_memory_usage)"
echo "  - Socket: $SOCKET_PATH"
echo "  - Storage file: $STORAGE_FILE"
echo ""

echo "========================================="
echo -e "${GREEN}Performance test completed!${NC}"
echo "========================================="