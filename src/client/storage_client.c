#include "../../include/client/storage_client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static uint32_t sequence_counter = 1;

// Connect to the storage daemon
int client_connect(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Failed to create socket");
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Failed to connect to daemon");
        close(fd);
        return -1;
    }
    
    return fd;
}

// Disconnect from the storage daemon
void client_disconnect(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

// Helper function to send a complete message
static int send_message(int fd, const struct message_header* header, const void* payload) {
    // Send header
    ssize_t bytes_sent = write(fd, header, sizeof(*header));
    if (bytes_sent != sizeof(*header)) {
        perror("Failed to send message header");
        return -1;
    }
    
    // Send payload if present
    if (header->payload_size > 0 && payload) {
        bytes_sent = write(fd, payload, header->payload_size);
        if (bytes_sent != header->payload_size) {
            perror("Failed to send message payload");
            return -1;
        }
    }
    
    return 0;
}

// Helper function to receive a complete message
static int receive_response(int fd, struct message_header* header, void** payload) {
    // Read header
    ssize_t bytes_read = read(fd, header, sizeof(*header));
    if (bytes_read != sizeof(*header)) {
        perror("Failed to read response header");
        return -1;
    }
    
    // Read payload if present
    *payload = NULL;
    if (header->payload_size > 0) {
        *payload = malloc(header->payload_size);
        if (!*payload) {
            perror("Failed to allocate payload buffer");
            return -1;
        }
        
        bytes_read = read(fd, *payload, header->payload_size);
        if (bytes_read != header->payload_size) {
            perror("Failed to read response payload");
            free(*payload);
            *payload = NULL;
            return -1;
        }
    }
    
    return 0;
}

// PUT operation
int client_put(int fd, const char* key, const char* value, size_t value_size) {
    if (!key || !value || strlen(key) >= MAX_KEY_SIZE) {
        return -1;
    }
    
    // Prepare request
    size_t payload_size = sizeof(struct put_request) + value_size;
    char* payload = malloc(payload_size);
    if (!payload) {
        return -1;
    }
    
    struct put_request* req = (struct put_request*)payload;
    memset(req->key, 0, MAX_KEY_SIZE);
    strncpy(req->key, key, MAX_KEY_SIZE - 1);
    req->value_size = value_size;
    
    // Copy value data after the request struct
    memcpy(payload + sizeof(struct put_request), value, value_size);
    
    // Prepare header
    struct message_header header = {
        .type = MSG_PUT_REQUEST,
        .payload_size = payload_size,
        .sequence_id = sequence_counter++,
        .reserved = 0
    };
    
    // Send request
    int result = send_message(fd, &header, payload);
    free(payload);
    
    if (result < 0) {
        return -1;
    }
    
    // Receive response
    struct message_header resp_header;
    void* resp_payload;
    result = receive_response(fd, &resp_header, &resp_payload);
    
    if (result < 0) {
        return -1;
    }
    
    // Check response type
    if (resp_header.type == MSG_PUT_RESPONSE) {
        struct put_response* resp = (struct put_response*)resp_payload;
        result = resp->result;
    } else if (resp_header.type == MSG_ERROR) {
        struct error_response* err = (struct error_response*)resp_payload;
        fprintf(stderr, "Server error: %s\n", err->error_message);
        result = err->error_code;
    } else {
        fprintf(stderr, "Unexpected response type: %u\n", resp_header.type);
        result = -1;
    }
    
    if (resp_payload) {
        free(resp_payload);
    }
    
    return result;
}

// GET operation
int client_get(int fd, const char* key, char* value, size_t* value_size) {
    if (!key || !value || !value_size || strlen(key) >= MAX_KEY_SIZE) {
        return -1;
    }
    
    // Prepare request
    struct get_request req;
    memset(req.key, 0, MAX_KEY_SIZE);
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    
    // Prepare header
    struct message_header header = {
        .type = MSG_GET_REQUEST,
        .payload_size = sizeof(struct get_request),
        .sequence_id = sequence_counter++,
        .reserved = 0
    };
    
    // Send request
    int result = send_message(fd, &header, &req);
    if (result < 0) {
        return -1;
    }
    
    // Receive response
    struct message_header resp_header;
    void* resp_payload;
    result = receive_response(fd, &resp_header, &resp_payload);
    
    if (result < 0) {
        return -1;
    }
    
    // Check response type
    if (resp_header.type == MSG_GET_RESPONSE) {
        struct get_response* resp = (struct get_response*)resp_payload;
        
        if (resp->result == 0) {
            // Success - check if we have enough buffer space
            if (*value_size < resp->value_size) {
                *value_size = resp->value_size;
                result = -1; // Buffer too small
            } else {
                // Copy value data (follows the get_response struct)
                char* value_data = (char*)resp_payload + sizeof(struct get_response);
                memcpy(value, value_data, resp->value_size);
                *value_size = resp->value_size;
                result = 0;
            }
        } else {
            result = resp->result; // Key not found or other error
        }
    } else if (resp_header.type == MSG_ERROR) {
        struct error_response* err = (struct error_response*)resp_payload;
        fprintf(stderr, "Server error: %s\n", err->error_message);
        result = err->error_code;
    } else {
        fprintf(stderr, "Unexpected response type: %u\n", resp_header.type);
        result = -1;
    }
    
    if (resp_payload) {
        free(resp_payload);
    }
    
    return result;
}

// DELETE operation
int client_delete(int fd, const char* key) {
    if (!key || strlen(key) >= MAX_KEY_SIZE) {
        return -1;
    }
    
    // Prepare request
    struct delete_request req;
    memset(req.key, 0, MAX_KEY_SIZE);
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    
    // Prepare header
    struct message_header header = {
        .type = MSG_DELETE_REQUEST,
        .payload_size = sizeof(struct delete_request),
        .sequence_id = sequence_counter++,
        .reserved = 0
    };
    
    // Send request
    int result = send_message(fd, &header, &req);
    if (result < 0) {
        return -1;
    }
    
    // Receive response
    struct message_header resp_header;
    void* resp_payload;
    result = receive_response(fd, &resp_header, &resp_payload);
    
    if (result < 0) {
        return -1;
    }
    
    // Check response type
    if (resp_header.type == MSG_DELETE_RESPONSE) {
        struct delete_response* resp = (struct delete_response*)resp_payload;
        result = resp->result;
    } else if (resp_header.type == MSG_ERROR) {
        struct error_response* err = (struct error_response*)resp_payload;
        fprintf(stderr, "Server error: %s\n", err->error_message);
        result = err->error_code;
    } else {
        fprintf(stderr, "Unexpected response type: %u\n", resp_header.type);
        result = -1;
    }
    
    if (resp_payload) {
        free(resp_payload);
    }
    
    return result;
}

// Helper for string PUT (adds null terminator)
int client_put_string(int fd, const char* key, const char* value) {
    if (!value) {
        return -1;
    }
    size_t value_len = strlen(value) + 1; // Include null terminator
    return client_put(fd, key, value, value_len);
}

// Helper for string GET (ensures null termination)
int client_get_string(int fd, const char* key, char* value, size_t value_buffer_size) {
    if (!value || value_buffer_size == 0) {
        return -1;
    }
    
    size_t actual_size = value_buffer_size;
    int result = client_get(fd, key, value, &actual_size);
    
    if (result == 0) {
        // Ensure null termination
        if (actual_size < value_buffer_size) {
            value[actual_size] = '\0';
        } else {
            value[value_buffer_size - 1] = '\0';
        }
    }
    
    return result;
}