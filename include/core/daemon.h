#ifndef CORE_DAEMON_H
#define CORE_DAEMON_H

#include <stdint.h>
#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif

// Protocol definitions (shared between C and C++)
#define SOCKET_PATH "/tmp/storage_daemon.sock"
#define MAX_CLIENTS 10
#define MAX_MESSAGE_SIZE 4096
#define MAX_VALUE_SIZE 4000  // Leave room for protocol headers

typedef enum {
    MSG_PUT_REQUEST = 1,
    MSG_PUT_RESPONSE = 2,
    MSG_GET_REQUEST = 3,
    MSG_GET_RESPONSE = 4,
    MSG_DELETE_REQUEST = 5,
    MSG_DELETE_RESPONSE = 6,
    MSG_ERROR = 7
} message_type_t;

struct message_header {
    uint32_t type;          // message_type_t
    uint32_t payload_size;  // Size of the payload following this header
    uint32_t sequence_id;   // For request/response matching
    uint32_t reserved;      // For future use
} __attribute__((packed));

// PUT request payload
struct put_request {
    char key[MAX_KEY_SIZE];
    uint32_t value_size;
    // Value data follows this struct
} __attribute__((packed));

// PUT response payload
struct put_response {
    int32_t result;  // 0 = success, negative = error code
} __attribute__((packed));

// GET request payload
struct get_request {
    char key[MAX_KEY_SIZE];
} __attribute__((packed));

// GET response payload
struct get_response {
    int32_t result;      // 0 = success, negative = error code
    uint32_t value_size; // Size of value data that follows
    // Value data follows this struct (if result == 0)
} __attribute__((packed));

// DELETE request payload
struct delete_request {
    char key[MAX_KEY_SIZE];
} __attribute__((packed));

// DELETE response payload
struct delete_response {
    int32_t result;  // 0 = success, negative = error code
} __attribute__((packed));

// Error response payload
struct error_response {
    int32_t error_code;
    char error_message[256];
} __attribute__((packed));

// Core daemon functions (C implementation)
int daemon_start(const char* storage_file);
int daemon_is_running(void);
void daemon_stop(void);

#ifdef __cplusplus
}
#endif

#endif // CORE_DAEMON_H
