#!/bin/bash

# Storage Daemon Stress Test
# Tests concurrent operations and performance

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

DAEMON_BIN="./build/storage_daemon"
CLIENT_BIN="./build/storage_client"
STORAGE_FILE="/tmp/stress_storage.db"
SOCKET_PATH="/tmp/storage_daemon.sock"

# Configuration
NUM_KEYS=1000
NUM_CONCURRENT=10
VALUE_SIZE=1024

# Clean up function
cleanup() {
    echo "Cleaning up..."
    pkill -f storage_daemon 2>/dev/null || true
    rm -f $STORAGE_FILE $SOCKET_PATH
    rm -f /tmp/stress_test_*.tmp
}

# Set up trap for cleanup
trap cleanup EXIT

# Start daemon
start_daemon() {
    echo "Starting storage daemon..."
    $DAEMON_BIN $STORAGE_FILE &
    sleep 2
    
    if ! pgrep -f storage_daemon > /dev/null; then
        echo -e "${RED}Failed to start daemon${NC}"
        exit 1
    fi
    echo -e "${GREEN}Daemon started${NC}"
}

# Generate random data
generate_value() {
    local size=$1
    head -c $size /dev/urandom | base64 | head -c $size
}

# Run concurrent operations
run_concurrent_test() {
    local operation=$1
    local num_ops=$2
    local prefix=$3
    
    echo -n "Running $num_ops concurrent $operation operations... "
    
    for i in $(seq 1 $num_ops); do
        {
            case $operation in
                "PUT")
                    value=$(generate_value $VALUE_SIZE)
                    $CLIENT_BIN put "${prefix}_key_$i" "$value" > /tmp/stress_test_$i.tmp 2>&1
                    ;;
                "GET")
                    $CLIENT_BIN get "${prefix}_key_$i" > /tmp/stress_test_$i.tmp 2>&1
                    ;;
                "DELETE")
                    $CLIENT_BIN delete "${prefix}_key_$i" > /tmp/stress_test_$i.tmp 2>&1
                    ;;
            esac
        } &
    done
    
    wait
    
    # Check results
    local failed=0
    for i in $(seq 1 $num_ops); do
        if ! grep -q "successful\|Value:" /tmp/stress_test_$i.tmp 2>/dev/null; then
            failed=$((failed + 1))
        fi
    done
    
    if [ $failed -eq 0 ]; then
        echo -e "${GREEN}PASSED${NC} (all $num_ops operations successful)"
    else
        echo -e "${YELLOW}PARTIAL${NC} ($failed/$num_ops operations failed)"
    fi
    
    rm -f /tmp/stress_test_*.tmp
}

# Measure operation performance
measure_performance() {
    local operation=$1
    local key=$2
    local value=$3
    
    start_time=$(date +%s%N)
    
    case $operation in
        "PUT")
            $CLIENT_BIN put "$key" "$value" > /dev/null 2>&1
            ;;
        "GET")
            $CLIENT_BIN get "$key" > /dev/null 2>&1
            ;;
        "DELETE")
            $CLIENT_BIN delete "$key" > /dev/null 2>&1
            ;;
    esac
    
    end_time=$(date +%s%N)
    elapsed=$((($end_time - $start_time) / 1000000))
    echo $elapsed
}

# Build the project
echo "Building project..."
make clean && make all
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Clean up any existing processes
cleanup

# Start daemon
start_daemon

echo ""
echo "================================"
echo "     STRESS TEST SUITE"
echo "================================"
echo ""

# Test 1: Sequential operations
echo -e "${YELLOW}Test 1: Sequential Operations${NC}"
echo "-------------------------------"

total_time=0
echo -n "PUT $NUM_KEYS keys sequentially... "
for i in $(seq 1 $NUM_KEYS); do
    value=$(generate_value 100)
    time=$(measure_performance "PUT" "seq_key_$i" "$value")
    total_time=$((total_time + time))
done
avg_time=$((total_time / NUM_KEYS))
echo -e "${GREEN}DONE${NC} (avg: ${avg_time}ms per operation)"

total_time=0
echo -n "GET $NUM_KEYS keys sequentially... "
for i in $(seq 1 $NUM_KEYS); do
    time=$(measure_performance "GET" "seq_key_$i" "")
    total_time=$((total_time + time))
done
avg_time=$((total_time / NUM_KEYS))
echo -e "${GREEN}DONE${NC} (avg: ${avg_time}ms per operation)"

echo ""

# Test 2: Concurrent operations
echo -e "${YELLOW}Test 2: Concurrent Operations${NC}"
echo "-------------------------------"
run_concurrent_test "PUT" $NUM_CONCURRENT "concurrent"
run_concurrent_test "GET" $NUM_CONCURRENT "concurrent"
run_concurrent_test "DELETE" $NUM_CONCURRENT "concurrent"

echo ""

# Test 3: Mixed workload
echo -e "${YELLOW}Test 3: Mixed Workload${NC}"
echo "-------------------------------"
echo -n "Running mixed operations... "

for i in $(seq 1 100); do
    {
        # Random operation
        op=$((RANDOM % 3))
        case $op in
            0) # PUT
                value=$(generate_value 256)
                $CLIENT_BIN put "mixed_key_$i" "$value" > /dev/null 2>&1
                ;;
            1) # GET
                $CLIENT_BIN get "mixed_key_$((RANDOM % i + 1))" > /dev/null 2>&1
                ;;
            2) # DELETE
                $CLIENT_BIN delete "mixed_key_$((RANDOM % i + 1))" > /dev/null 2>&1
                ;;
        esac
    } &
done

wait
echo -e "${GREEN}DONE${NC}"

echo ""

# Test 4: Large values
echo -e "${YELLOW}Test 4: Large Value Handling${NC}"
echo "-------------------------------"

for size in 1024 4096 16384 65536; do
    echo -n "Testing ${size}B value... "
    large_value=$(generate_value $size)
    
    start_time=$(date +%s%N)
    $CLIENT_BIN put "large_key_$size" "$large_value" > /dev/null 2>&1
    put_time=$(($(date +%s%N) - start_time))
    
    start_time=$(date +%s%N)
    $CLIENT_BIN get "large_key_$size" > /dev/null 2>&1
    get_time=$(($(date +%s%N) - start_time))
    
    put_ms=$((put_time / 1000000))
    get_ms=$((get_time / 1000000))
    
    echo -e "${GREEN}DONE${NC} (PUT: ${put_ms}ms, GET: ${get_ms}ms)"
done

echo ""

# Test 5: Persistence check
echo -e "${YELLOW}Test 5: Persistence Check${NC}"
echo "-------------------------------"

echo -n "Adding test data... "
$CLIENT_BIN put "persist_key" "persist_value" > /dev/null 2>&1
echo -e "${GREEN}DONE${NC}"

echo -n "Restarting daemon... "
pkill -f storage_daemon 2>/dev/null || true
sleep 1
start_daemon > /dev/null 2>&1
echo -e "${GREEN}DONE${NC}"

echo -n "Verifying data persistence... "
result=$($CLIENT_BIN get "persist_key" 2>&1)
if [[ "$result" == *"persist_value"* ]]; then
    echo -e "${GREEN}PASSED${NC}"
else
    echo -e "${RED}FAILED${NC}"
fi

echo ""
echo "================================"
echo -e "${GREEN}Stress test completed!${NC}"
echo "================================"