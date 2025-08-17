CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Wpedantic -g -pthread
CXXFLAGS = -Wall -Wextra -Wpedantic -g -pthread -std=c++17
LDFLAGS = -pthread

# Directories
SRCDIR = src
COREDIR = $(SRCDIR)/core
SERVERDIR = $(SRCDIR)/server
CLIENTDIR = $(SRCDIR)/client
INCDIR = include
BINDIR = bin
OBJDIR = obj

# Create directories if they don't exist
$(shell mkdir -p $(BINDIR) $(OBJDIR) $(OBJDIR)/core $(OBJDIR)/server $(OBJDIR)/client)

# Targets
all: $(BINDIR)/storage_daemon $(BINDIR)/storage_client

# Storage daemon
$(BINDIR)/storage_daemon: $(OBJDIR)/core/main.o $(OBJDIR)/core/daemon.o $(OBJDIR)/core/storage.o
	$(CC) $(CFLAGS) -o $@ $(OBJDIR)/core/main.o $(OBJDIR)/core/daemon.o $(OBJDIR)/core/storage.o $(LDFLAGS)

# Storage client
$(BINDIR)/storage_client: $(OBJDIR)/client/cli.o $(OBJDIR)/client/storage_client.o
	$(CC) $(CFLAGS) -o $@ $(OBJDIR)/client/cli.o $(OBJDIR)/client/storage_client.o $(LDFLAGS)

# Core C objects
$(OBJDIR)/core/storage.o: $(COREDIR)/storage.c $(INCDIR)/core/storage.h
	$(CC) $(CFLAGS) -c -o $@ $(COREDIR)/storage.c

$(OBJDIR)/core/daemon.o: $(COREDIR)/daemon.c $(INCDIR)/core/daemon.h
	$(CC) $(CFLAGS) -c -o $@ $(COREDIR)/daemon.c

$(OBJDIR)/core/main.o: $(COREDIR)/main.c $(INCDIR)/core/daemon.h
	$(CC) $(CFLAGS) -c -o $@ $(COREDIR)/main.c

# Server C++ objects
$(OBJDIR)/server/StorageEngine.o: $(SERVERDIR)/StorageEngine.cpp $(INCDIR)/server/StorageEngine.hpp $(INCDIR)/core/storage.h
	$(CXX) $(CXXFLAGS) -c -o $@ $(SERVERDIR)/StorageEngine.cpp

# Client C objects
$(OBJDIR)/client/storage_client.o: $(CLIENTDIR)/storage_client.c $(INCDIR)/client/storage_client.h $(INCDIR)/core/daemon.h
	$(CC) $(CFLAGS) -c -o $@ $(CLIENTDIR)/storage_client.c

$(OBJDIR)/client/cli.o: $(CLIENTDIR)/cli.c $(INCDIR)/client/storage_client.h
	$(CC) $(CFLAGS) -c -o $@ $(CLIENTDIR)/cli.c

# Run tests
test: all
	./tests/test.sh

test-stress: all
	./tests/stress_test.sh

test-performance: all
	./tests/performance_test.sh

test-all: test test-stress test-performance

# Clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)

.PHONY: all test test-stress test-performance test-all clean