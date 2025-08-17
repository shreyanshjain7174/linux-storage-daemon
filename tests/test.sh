#!/bin/bash

# Storage Daemon Test Suite
# Tests basic PUT/GET/DELETE operations

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

DAEMON_BIN="./build/storage_daemon"
CLIENT_BIN="./build/storage_client"
STORAGE_FILE="/tmp/test_storage.db"
SOCKET_PATH="/tmp/storage_daemon.sock"

# Clean up function
cleanup() {
    echo "Cleaning up..."
    pkill -f storage_daemon 2>/dev/null || true
    rm -f $STORAGE_FILE $SOCKET_PATH
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

# Run test
run_test() {
    local test_name=$1
    local cmd=$2
    local expected=$3
    
    echo -n "Testing $test_name... "
    
    result=$($cmd 2>&1)
    if [[ "$result" == *"$expected"* ]]; then
        echo -e "${GREEN}PASSED${NC}"
        return 0
    else
        echo -e "${RED}FAILED${NC}"
        echo "  Expected: $expected"
        echo "  Got: $result"
        return 1
    fi
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

# Run tests
echo ""
echo "Running tests..."
echo "==============="

# Test 1: PUT operation
run_test "PUT simple value" "$CLIENT_BIN put key1 value1" "PUT successful"

# Test 2: GET operation
run_test "GET existing key" "$CLIENT_BIN get key1" "value1"

# Test 3: PUT with spaces
run_test "PUT value with spaces" "$CLIENT_BIN put key2 'hello world'" "PUT successful"

# Test 4: GET value with spaces
run_test "GET value with spaces" "$CLIENT_BIN get key2" "hello world"

# Test 5: DELETE operation
run_test "DELETE existing key" "$CLIENT_BIN delete key1" "DELETE successful"

# Test 6: GET deleted key
run_test "GET deleted key" "$CLIENT_BIN get key1" "Key not found"

# Test 7: DELETE non-existent key
run_test "DELETE non-existent key" "$CLIENT_BIN delete nonexistent" "Key not found"

# Test 8: Large value
large_value=$(printf 'x%.0s' {1..1000})
run_test "PUT large value" "$CLIENT_BIN put bigkey '$large_value'" "PUT successful"
run_test "GET large value" "$CLIENT_BIN get bigkey" "$large_value"

# Test 9: Empty value
run_test "PUT empty value" "$CLIENT_BIN put emptykey ''" "PUT successful"
run_test "GET empty value" "$CLIENT_BIN get emptykey" "Value: "

# Test 10: Overwrite existing key
run_test "Overwrite existing key" "$CLIENT_BIN put key2 newvalue" "PUT successful"
run_test "GET overwritten key" "$CLIENT_BIN get key2" "newvalue"

echo ""
echo "==============="
echo -e "${GREEN}All tests completed!${NC}"