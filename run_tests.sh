#!/bin/bash

# Main test runner script for Docker container
# Runs all tests in a Linux environment

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

IMAGE_NAME="storage-daemon-test"
CONTAINER_NAME="storage-daemon-test-container"

# Build Docker image
build_docker_image() {
    echo -e "${BLUE}Building Docker image...${NC}"
    
    # Create Dockerfile if it doesn't exist
    cat > Dockerfile << 'EOF'
FROM ubuntu:22.04

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    bc \
    procps \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN make clean && make all

# Make test scripts executable
RUN chmod +x tests/*.sh run_tests.sh
EOF

    docker build -t $IMAGE_NAME . > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Docker image built successfully${NC}"
    else
        echo -e "${RED}Failed to build Docker image${NC}"
        exit 1
    fi
}

# Run tests in container
run_in_container() {
    local test_script=$1
    local test_name=$2
    
    echo ""
    echo -e "${YELLOW}Running $test_name...${NC}"
    echo "================================"
    
    docker run --rm \
        --name $CONTAINER_NAME \
        -v $(pwd):/app \
        $IMAGE_NAME \
        bash -c "cd /app && $test_script"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}$test_name completed successfully${NC}"
    else
        echo -e "${RED}$test_name failed${NC}"
        return 1
    fi
}

# Main execution
main() {
    echo ""
    echo "========================================="
    echo "   STORAGE DAEMON TEST SUITE (DOCKER)"
    echo "========================================="
    echo ""
    
    # Check if Docker is installed
    if ! command -v docker &> /dev/null; then
        echo -e "${RED}Docker is not installed. Please install Docker first.${NC}"
        exit 1
    fi
    
    # Check if Docker daemon is running
    if ! docker info > /dev/null 2>&1; then
        echo -e "${RED}Docker daemon is not running. Please start Docker.${NC}"
        exit 1
    fi
    
    # Build Docker image
    build_docker_image
    
    # Run each test suite
    local all_passed=true
    
    # Basic functionality tests
    if ! run_in_container "./tests/test.sh" "Basic Functionality Tests"; then
        all_passed=false
    fi
    
    # Stress tests
    if ! run_in_container "./tests/stress_test.sh" "Stress Tests"; then
        all_passed=false
    fi
    
    # Performance tests
    if ! run_in_container "./tests/performance_test.sh" "Performance Tests"; then
        all_passed=false
    fi
    
    echo ""
    echo "========================================="
    
    if [ "$all_passed" = true ]; then
        echo -e "${GREEN}ALL TESTS PASSED!${NC}"
        echo "========================================="
        exit 0
    else
        echo -e "${RED}SOME TESTS FAILED${NC}"
        echo "========================================="
        exit 1
    fi
}

# Handle script arguments
if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [test_name]"
    echo ""
    echo "Run storage daemon tests in a Docker container"
    echo ""
    echo "Options:"
    echo "  --help, -h     Show this help message"
    echo ""
    echo "Test names (optional):"
    echo "  basic          Run basic functionality tests only"
    echo "  stress         Run stress tests only"
    echo "  performance    Run performance tests only"
    echo ""
    echo "If no test name is provided, all tests will be run."
    exit 0
fi

# Run specific test if requested
if [ "$1" == "basic" ]; then
    build_docker_image
    run_in_container "./tests/test.sh" "Basic Functionality Tests"
elif [ "$1" == "stress" ]; then
    build_docker_image
    run_in_container "./tests/stress_test.sh" "Stress Tests"
elif [ "$1" == "performance" ]; then
    build_docker_image
    run_in_container "./tests/performance_test.sh" "Performance Tests"
else
    # Run all tests
    main
fi