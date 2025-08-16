#!/bin/bash

# Linux testing script using Docker

set -e

echo "Building Docker image for Linux testing..."
docker build -t kvstore-daemon .

if [ "$1" = "daemon" ]; then
    echo "Running full daemon test suite in Linux container..."
    docker run --rm -v "$(pwd)":/app -w /app kvstore-daemon bash -c "
        make clean
        make all
        ./test.sh
    "
else
    echo "Running storage engine test in Linux container..."
    docker run --rm -v "$(pwd)":/app -w /app kvstore-daemon bash -c "
        make clean
        make test
    "
fi

echo "Linux testing complete!"