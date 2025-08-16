#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/client/storage_client.h"

void show_usage(const char* program_name) {
    printf("Usage: %s <command> [arguments]\n", program_name);
    printf("\nCommands:\n");
    printf("  put <key> <value>    Store a key-value pair\n");
    printf("  get <key>            Retrieve value for a key\n");
    printf("  delete <key>         Delete a key-value pair\n");
    printf("\nExamples:\n");
    printf("  %s put mykey \"my value\"\n", program_name);
    printf("  %s get mykey\n", program_name);
    printf("  %s delete mykey\n", program_name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        show_usage(argv[0]);
        return 1;
    }
    
    // Connect to daemon
    int fd = client_connect();
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to storage daemon\n");
        fprintf(stderr, "Make sure the daemon is running\n");
        return 1;
    }
    
    const char* command = argv[1];
    int result = 0;
    
    if (strcmp(command, "put") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s put <key> <value>\n", argv[0]);
            client_disconnect(fd);
            return 1;
        }
        
        const char* key = argv[2];
        const char* value = argv[3];
        
        printf("Storing key='%s' value='%s'\n", key, value);
        result = client_put_string(fd, key, value);
        
        if (result == 0) {
            printf("✓ PUT successful\n");
        } else {
            printf("✗ PUT failed (error %d)\n", result);
        }
        
    } else if (strcmp(command, "get") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s get <key>\n", argv[0]);
            client_disconnect(fd);
            return 1;
        }
        
        const char* key = argv[2];
        char value[4096]; // Buffer for retrieved value
        
        printf("Retrieving key='%s'\n", key);
        result = client_get_string(fd, key, value, sizeof(value));
        
        if (result == 0) {
            printf("✓ GET successful\n");
            printf("Value: %s\n", value);
        } else if (result == -1) {
            printf("✗ Key not found\n");
        } else {
            printf("✗ GET failed (error %d)\n", result);
        }
        
    } else if (strcmp(command, "delete") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s delete <key>\n", argv[0]);
            client_disconnect(fd);
            return 1;
        }
        
        const char* key = argv[2];
        
        printf("Deleting key='%s'\n", key);
        result = client_delete(fd, key);
        
        if (result == 0) {
            printf("✓ DELETE successful\n");
        } else if (result == -1) {
            printf("✗ Key not found\n");
        } else {
            printf("✗ DELETE failed (error %d)\n", result);
        }
        
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        show_usage(argv[0]);
        client_disconnect(fd);
        return 1;
    }
    
    client_disconnect(fd);
    return (result == 0) ? 0 : 1;
}