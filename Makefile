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
all: $(BINDIR)/test_storage $(BINDIR)/test_storage_cpp $(BINDIR)/storage_daemon $(BINDIR)/storage_client

# Original C test program
$(BINDIR)/test_storage: test_storage.c $(OBJDIR)/core/storage.o
	$(CC) $(CFLAGS) -o $@ test_storage.c $(OBJDIR)/core/storage.o $(LDFLAGS)

# C++ test program
$(BINDIR)/test_storage_cpp: test_storage_cpp.cpp $(OBJDIR)/core/storage.o $(OBJDIR)/server/StorageEngine.o
	$(CXX) $(CXXFLAGS) -o $@ test_storage_cpp.cpp $(OBJDIR)/core/storage.o $(OBJDIR)/server/StorageEngine.o $(LDFLAGS)

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
test: $(BINDIR)/test_storage
	./$(BINDIR)/test_storage

test-cpp: $(BINDIR)/test_storage_cpp
	./$(BINDIR)/test_storage_cpp

test-all: test test-cpp

# Clean
clean:
	rm -rf $(BINDIR) $(OBJDIR)
	rm -f test_storage.db test_storage_cpp.db

.PHONY: all test test-cpp test-all clean