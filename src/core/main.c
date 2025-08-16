#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/core/daemon.h"

void show_usage(const char* program_name) {
    printf("Usage: %s [options] <storage_file>\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help     Show this help message\n");
    printf("\nArguments:\n");
    printf("  storage_file   Path to the storage file (will be created if it doesn't exist)\n");
    printf("\nExample:\n");
    printf("  %s /var/lib/storage/data.db\n", program_name);
    printf("  %s ./storage.db\n", program_name);
    printf("\nThe daemon will:\n");
    printf("  - Run in the background\n");
    printf("  - Listen on /tmp/storage_daemon.sock\n");
    printf("  - Log to syslog\n");
    printf("  - Handle SIGTERM/SIGINT for graceful shutdown\n");
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }
    
    // Check for help flag
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        show_usage(argv[0]);
        return 0;
    }
    
    const char* storage_file = argv[1];
    
    // Validate storage file path
    if (strlen(storage_file) == 0) {
        fprintf(stderr, "Error: Storage file path cannot be empty\n");
        return 1;
    }
    
    printf("Starting storage daemon with file: %s\n", storage_file);
    printf("The daemon will run in the background.\n");
    printf("Check syslog for daemon messages: sudo tail -f /var/log/syslog | grep storage_daemon\n");
    printf("Connect using: ./storage_client put key value\n");
    
    // Start the daemon
    int result = daemon_start(storage_file);
    
    if (result != 0) {
        fprintf(stderr, "Failed to start daemon\n");
        return 1;
    }
    
    return 0;
}