FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    && rm -rf /var/lib/apt/lists/*

# Create working directory
WORKDIR /app

# Default command for interactive use
CMD ["/bin/bash"]