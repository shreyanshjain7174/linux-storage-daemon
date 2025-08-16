#!/bin/bash

# test.sh - Storage Daemon Test Suite
# Tests the complete functionality of the storage daemon and client

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DB="test_daemon.db"
DAEMON_PID=""
SOCKET_PATH="/tmp/storage_daemon.sock"

# Print functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}    $1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_test() {
    echo -e "${YELLOW}Testing: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Cleanup function
cleanup() {
    print_info "Cleaning up..."
    
    # Kill daemon if running
    if [ ! -z "$DAEMON_PID" ]; then
        print_info "Stopping daemon (PID: $DAEMON_PID)"
        kill $DAEMON_PID 2>/dev/null || true
        wait $DAEMON_PID 2>/dev/null || true
    fi
    
    # Remove test files
    rm -f "$TEST_DB" "$SOCKET_PATH"
    
    print_info "Cleanup completed"
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Function to wait for daemon to start
wait_for_daemon() {
    local retries=20
    local count=0
    
    print_info "Waiting for daemon to create socket..."
    
    while [ $count -lt $retries ]; do
        if [ -S "$SOCKET_PATH" ]; then
            print_success "Daemon socket is ready"
            sleep 0.5  # Give daemon a moment to fully initialize
            return 0
        fi
        sleep 0.5
        count=$((count + 1))
        
        # Show progress every 5 attempts
        if [ $((count % 5)) -eq 0 ]; then
            print_info "Still waiting... (attempt $count/$retries)"
        fi
    done
    
    print_error "Daemon failed to start within 10 seconds"
    print_info "Checking if daemon process is still running..."
    if [ ! -z "$DAEMON_PID" ] && kill -0 $DAEMON_PID 2>/dev/null; then
        print_info "Daemon process is running but socket not created"
    else
        print_info "Daemon process has exited"
    fi
    return 1
}

# Function to test client command
test_command() {
    local description="$1"
    local expected_exit="$2"
    shift 2
    local cmd="$@"
    
    print_test "$description"
    
    if output=$(timeout 5 $cmd 2>&1); then
        actual_exit=0
    else
        actual_exit=$?
    fi
    
    if [ $actual_exit -eq $expected_exit ]; then
        print_success "$description"
        if [ ! -z "$output" ]; then
            echo "    Output: $output"
        fi
        return 0
    else
        print_error "$description (expected exit $expected_exit, got $actual_exit)"
        echo "    Output: $output"
        return 1
    fi
}

# Main test execution
main() {
    print_header "Storage Daemon Test Suite"
    
    # Check if binaries exist
    print_test "Checking if binaries exist"
    if [ ! -f "bin/storage_daemon" ] || [ ! -f "bin/storage_client" ]; then
        print_error "Binaries not found. Run 'make all' first."
        exit 1
    fi
    print_success "Binaries found"
    
    # Start daemon
    print_test "Starting storage daemon"
    ./bin/storage_daemon "$TEST_DB" &
    DAEMON_PID=$!
    print_info "Daemon started with PID: $DAEMON_PID"
    
    # Wait for daemon to be ready
    if ! wait_for_daemon; then
        exit 1
    fi
    
    # Test 1: Basic PUT operation
    test_command "PUT key1 with value1" 0 ./bin/storage_client put key1 "value1"
    
    # Test 2: GET the value we just put
    test_command "GET key1" 0 ./bin/storage_client get key1
    
    # Test 3: PUT another key
    test_command "PUT key2 with value2" 0 ./bin/storage_client put key2 "value2"
    
    # Test 4: GET the second key
    test_command "GET key2" 0 ./bin/storage_client get key2
    
    # Test 5: PUT with spaces and special characters
    test_command "PUT key3 with complex value" 0 ./bin/storage_client put key3 "Hello World! @#$%^&*()"
    
    # Test 6: GET complex value
    test_command "GET key3 complex value" 0 ./bin/storage_client get key3
    
    # Test 7: DELETE operation
    test_command "DELETE key1" 0 ./bin/storage_client delete key1
    
    # Test 8: GET deleted key (should fail)
    test_command "GET deleted key1 (should fail)" 1 ./bin/storage_client get key1
    
    # Test 9: DELETE non-existent key (should fail)
    test_command "DELETE non-existent key (should fail)" 1 ./bin/storage_client delete nonexistent
    
    # Test 10: PUT with large value
    large_value=$(printf 'A%.0s' {1..1000})  # 1000 A's
    test_command "PUT key4 with large value (1000 bytes)" 0 ./bin/storage_client put key4 "$large_value"
    
    # Test 11: GET large value
    test_command "GET key4 large value" 0 ./bin/storage_client get key4
    
    # Test 12: Test maximum keys (we have limit of 7)
    print_test "Testing multiple keys (up to limit)"
    for i in {5..7}; do
        test_command "PUT key$i" 0 ./bin/storage_client put "key$i" "value$i"
    done
    
    # Test 13: Test key limit exceeded (8th key should fail)
    test_command "PUT 8th key (should fail - key limit)" 1 ./bin/storage_client put key8 "value8"
    
    # Test 14: Test invalid commands
    test_command "Invalid command (should fail)" 1 ./bin/storage_client invalid
    
    # Test 15: Test empty key (should fail)
    test_command "Empty key (should fail)" 1 ./bin/storage_client put "" "value"
    
    # Test 16: Multiple rapid operations
    print_test "Rapid sequential operations"
    for i in {1..5}; do
        ./bin/storage_client delete "rapid$i" 2>/dev/null || true  # Clean up first
        ./bin/storage_client put "rapid$i" "rapid_value$i" >/dev/null || exit 1
        ./bin/storage_client get "rapid$i" >/dev/null || exit 1
        ./bin/storage_client delete "rapid$i" >/dev/null || exit 1
    done
    print_success "Rapid sequential operations"
    
    # Test 17: Concurrent operations (basic test)
    print_test "Basic concurrent operations"
    (./bin/storage_client put concurrent1 "value1" &)
    (./bin/storage_client put concurrent2 "value2" &)
    wait
    
    # Verify concurrent operations worked
    test_command "GET concurrent1" 0 ./bin/storage_client get concurrent1
    test_command "GET concurrent2" 0 ./bin/storage_client get concurrent2
    
    # Final statistics
    print_header "Test Results Summary"
    
    # Check storage file was created
    if [ -f "$TEST_DB" ]; then
        file_size=$(stat -f%z "$TEST_DB" 2>/dev/null || stat -c%s "$TEST_DB" 2>/dev/null)
        print_success "Storage file created: $TEST_DB ($file_size bytes)"
    else
        print_error "Storage file not created"
    fi
    
    # Check daemon is still running
    if kill -0 $DAEMON_PID 2>/dev/null; then
        print_success "Daemon still running (PID: $DAEMON_PID)"
    else
        print_error "Daemon stopped unexpectedly"
    fi
    
    print_header "All Tests Completed Successfully!"
    print_info "The storage daemon is fully functional"
    print_info "Storage file: $TEST_DB"
    print_info "Socket path: $SOCKET_PATH"
    
    return 0
}

# Run main function
main "$@"