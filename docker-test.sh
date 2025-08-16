#!/bin/bash

# Linux testing script using Docker

set -e

echo "Building Docker image for Linux testing..."
docker build -t kvstore-daemon .

echo "Running storage engine test in Linux container..."
docker run --rm -v "$(pwd)":/app -w /app kvstore-daemon bash -c "
    make clean
    make test
"

echo "Linux testing complete!"