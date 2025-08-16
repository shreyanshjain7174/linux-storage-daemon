#ifndef CLIENT_STORAGE_CLIENT_H
#define CLIENT_STORAGE_CLIENT_H

#include <stddef.h>
#include "../core/daemon.h"

#ifdef __cplusplus
extern "C" {
#endif

// Client connection management
int client_connect(void);
void client_disconnect(int fd);

// Storage operations
int client_put(int fd, const char* key, const char* value, size_t value_size);
int client_get(int fd, const char* key, char* value, size_t* value_size);
int client_delete(int fd, const char* key);

// Helper for string values
int client_put_string(int fd, const char* key, const char* value);
int client_get_string(int fd, const char* key, char* value, size_t value_buffer_size);

#ifdef __cplusplus
}
#endif

#endif // CLIENT_STORAGE_CLIENT_H