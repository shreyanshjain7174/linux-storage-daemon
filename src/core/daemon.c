#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include "../../include/core/daemon.h"
#include "../../include/core/storage.h"

// Global daemon state
static int server_socket = -1;
static volatile int daemon_running = 0;
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static int create_daemon_process(void);
static int setup_unix_socket(void);
static void handle_signal(int sig);
static void cleanup_daemon(void);
static int process_message(int client_fd);

// Signal handler for graceful shutdown
static void handle_signal(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            syslog(LOG_INFO, "Received shutdown signal %d", sig);
            daemon_running = 0;
            break;
        case SIGHUP:
            syslog(LOG_INFO, "Received SIGHUP - ignoring for now");
            break;
        default:
            syslog(LOG_WARNING, "Received unexpected signal %d", sig);
            break;
    }
}

// Create daemon process using the standard double-fork technique
static int create_daemon_process(void) {
    pid_t pid, sid;
    
    // First fork
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
        return -1;
    }
    
    // Parent process exits
    if (pid > 0) {
        printf("Daemon started with PID: %d\n", pid);
        exit(0);
    }
    
    // Child process continues - create new session
    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "Failed to create session: %s\n", strerror(errno));
        return -1;
    }
    
    // Second fork to ensure we're not session leader
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork second time: %s\n", strerror(errno));
        return -1;
    }
    
    // First child exits
    if (pid > 0) {
        exit(0);
    }
    
    // Change working directory to root to avoid locking any directory
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Failed to change directory to /: %s", strerror(errno));
        return -1;
    }
    
    // Set file permissions mask
    umask(0);
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect standard file descriptors to /dev/null
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd >= 0) {
        dup2(null_fd, STDIN_FILENO);
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        if (null_fd > STDERR_FILENO) {
            close(null_fd);
        }
    }
    
    return 0;
}

// Setup UNIX domain socket server
static int setup_unix_socket(void) {
    struct sockaddr_un addr;
    
    // Create socket
    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    // Remove existing socket file if it exists
    unlink(SOCKET_PATH);
    
    // Setup address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    // Set socket permissions
    chmod(SOCKET_PATH, 0666);
    
    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_socket);
        return -1;
    }
    
    syslog(LOG_INFO, "Socket server listening on %s", SOCKET_PATH);
    return 0;
}

// Cleanup resources
static void cleanup_daemon(void) {
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    unlink(SOCKET_PATH);
    storage_cleanup();
    syslog(LOG_INFO, "Daemon cleanup completed");
    closelog();
}

// Main daemon entry point
int daemon_start(const char* storage_file) {
    // Setup signal handlers before becoming daemon
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals
    
    // Open syslog
    openlog("storage_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Starting storage daemon");
    
    // Create daemon process
    if (create_daemon_process() < 0) {
        syslog(LOG_ERR, "Failed to create daemon process");
        return -1;
    }
    
    // Initialize storage
    if (storage_init(storage_file) < 0) {
        syslog(LOG_ERR, "Failed to initialize storage");
        return -1;
    }
    
    // Setup socket server
    if (setup_unix_socket() < 0) {
        syslog(LOG_ERR, "Failed to setup socket server");
        storage_cleanup();
        return -1;
    }
    
    // Set daemon as running
    daemon_running = 1;
    syslog(LOG_INFO, "Daemon started successfully");
    
    // Main server loop
    while (daemon_running) {
        fd_set read_fds;
        struct timeval timeout;
        
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        // Set timeout for select (1 second)
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "Select error: %s", strerror(errno));
                break;
            }
            continue;
        }
        
        if (activity == 0) {
            // Timeout - check if we should continue running
            continue;
        }
        
        if (FD_ISSET(server_socket, &read_fds)) {
            // Accept new connection
            int client_fd = accept(server_socket, NULL, NULL);
            if (client_fd < 0) {
                if (errno != EINTR) {
                    syslog(LOG_ERR, "Accept error: %s", strerror(errno));
                }
                continue;
            }
            
            // For now, handle client synchronously
            // TODO: In the threading step, we'll create a thread for each client
            if (process_message(client_fd) < 0) {
                syslog(LOG_WARNING, "Failed to process client message");
            }
            
            close(client_fd);
        }
    }
    
    cleanup_daemon();
    return 0;
}

// Process a single message from client
static int process_message(int client_fd) {
    struct message_header header;
    
    // Read message header
    ssize_t bytes_read = read(client_fd, &header, sizeof(header));
    if (bytes_read != sizeof(header)) {
        syslog(LOG_WARNING, "Failed to read message header");
        return -1;
    }
    
    syslog(LOG_DEBUG, "Received message type %d, payload size %d", 
           header.type, header.payload_size);
    
    // Validate payload size
    if (header.payload_size > MAX_MESSAGE_SIZE) {
        syslog(LOG_WARNING, "Payload size too large: %u", header.payload_size);
        return -1;
    }
    
    // Allocate buffer for payload
    char* payload = NULL;
    if (header.payload_size > 0) {
        payload = malloc(header.payload_size);
        if (!payload) {
            syslog(LOG_ERR, "Failed to allocate payload buffer");
            return -1;
        }
        
        // Read payload
        bytes_read = read(client_fd, payload, header.payload_size);
        if (bytes_read != header.payload_size) {
            syslog(LOG_WARNING, "Failed to read payload");
            free(payload);
            return -1;
        }
    }
    
    // Process based on message type
    switch (header.type) {
        case MSG_PUT_REQUEST: {
            struct put_request* req = (struct put_request*)payload;
            
            // Validate request
            if (header.payload_size < sizeof(struct put_request)) {
                syslog(LOG_WARNING, "Invalid PUT request size");
                free(payload);
                return -1;
            }
            
            // Extract value data (follows the put_request struct)
            char* value = payload + sizeof(struct put_request);
            size_t expected_size = sizeof(struct put_request) + req->value_size;
            
            if (header.payload_size != expected_size) {
                syslog(LOG_WARNING, "PUT request size mismatch");
                free(payload);
                return -1;
            }
            
            // Call storage function with mutex protection
            pthread_mutex_lock(&storage_mutex);
            int result = storage_put(req->key, value, req->value_size);
            pthread_mutex_unlock(&storage_mutex);
            
            syslog(LOG_INFO, "PUT key='%s' value_size=%u result=%d", 
                   req->key, req->value_size, result);
            
            // Send response
            struct message_header resp_header = {
                .type = MSG_PUT_RESPONSE,
                .payload_size = sizeof(struct put_response),
                .sequence_id = header.sequence_id,
                .reserved = 0
            };
            
            struct put_response resp = {
                .result = result
            };
            
            write(client_fd, &resp_header, sizeof(resp_header));
            write(client_fd, &resp, sizeof(resp));
            break;
        }
        
        case MSG_GET_REQUEST: {
            struct get_request* req = (struct get_request*)payload;
            
            // Validate request
            if (header.payload_size != sizeof(struct get_request)) {
                syslog(LOG_WARNING, "Invalid GET request size");
                free(payload);
                return -1;
            }
            
            // First, get the value size
            size_t value_size = 0;
            pthread_mutex_lock(&storage_mutex);
            int result = storage_get(req->key, NULL, &value_size);
            
            if (result == 0) {
                // Allocate buffer for value
                char* value_buffer = malloc(value_size);
                if (value_buffer) {
                    // Get the actual value
                    result = storage_get(req->key, value_buffer, &value_size);
                    pthread_mutex_unlock(&storage_mutex);
                    
                    if (result == 0) {
                        syslog(LOG_INFO, "GET key='%s' value_size=%zu result=%d", 
                               req->key, value_size, result);
                        
                        // Send success response with value
                        struct message_header resp_header = {
                            .type = MSG_GET_RESPONSE,
                            .payload_size = sizeof(struct get_response) + value_size,
                            .sequence_id = header.sequence_id,
                            .reserved = 0
                        };
                        
                        struct get_response resp = {
                            .result = 0,
                            .value_size = value_size
                        };
                        
                        // Send response header and response struct
                        write(client_fd, &resp_header, sizeof(resp_header));
                        write(client_fd, &resp, sizeof(resp));
                        
                        // Send value data
                        if (value_size > 0) {
                            write(client_fd, value_buffer, value_size);
                        }
                    } else {
                        // Error reading value
                        syslog(LOG_WARNING, "GET key='%s' failed to read value: %d", 
                               req->key, result);
                        
                        struct message_header resp_header = {
                            .type = MSG_GET_RESPONSE,
                            .payload_size = sizeof(struct get_response),
                            .sequence_id = header.sequence_id,
                            .reserved = 0
                        };
                        
                        struct get_response resp = {
                            .result = result,
                            .value_size = 0
                        };
                        
                        write(client_fd, &resp_header, sizeof(resp_header));
                        write(client_fd, &resp, sizeof(resp));
                    }
                    
                    free(value_buffer);
                } else {
                    pthread_mutex_unlock(&storage_mutex);
                    syslog(LOG_ERR, "Failed to allocate value buffer for GET");
                    result = -1;
                }
            } else {
                pthread_mutex_unlock(&storage_mutex);
                syslog(LOG_INFO, "GET key='%s' not found: %d", req->key, result);
            }
            
            // If we get here, it's an error case
            if (result != 0) {
                struct message_header resp_header = {
                    .type = MSG_GET_RESPONSE,
                    .payload_size = sizeof(struct get_response),
                    .sequence_id = header.sequence_id,
                    .reserved = 0
                };
                
                struct get_response resp = {
                    .result = result,
                    .value_size = 0
                };
                
                write(client_fd, &resp_header, sizeof(resp_header));
                write(client_fd, &resp, sizeof(resp));
            }
            break;
        }
        
        case MSG_DELETE_REQUEST: {
            struct delete_request* req = (struct delete_request*)payload;
            
            // Validate request
            if (header.payload_size != sizeof(struct delete_request)) {
                syslog(LOG_WARNING, "Invalid DELETE request size");
                free(payload);
                return -1;
            }
            
            // Call storage function with mutex protection
            pthread_mutex_lock(&storage_mutex);
            int result = storage_delete(req->key);
            pthread_mutex_unlock(&storage_mutex);
            
            syslog(LOG_INFO, "DELETE key='%s' result=%d", req->key, result);
            
            // Send response
            struct message_header resp_header = {
                .type = MSG_DELETE_RESPONSE,
                .payload_size = sizeof(struct delete_response),
                .sequence_id = header.sequence_id,
                .reserved = 0
            };
            
            struct delete_response resp = {
                .result = result
            };
            
            write(client_fd, &resp_header, sizeof(resp_header));
            write(client_fd, &resp, sizeof(resp));
            break;
        }
        
        default: {
            syslog(LOG_WARNING, "Unknown message type: %u", header.type);
            
            struct message_header resp_header = {
                .type = MSG_ERROR,
                .payload_size = sizeof(struct error_response),
                .sequence_id = header.sequence_id,
                .reserved = 0
            };
            
            struct error_response error_resp = {
                .error_code = -1
            };
            snprintf(error_resp.error_message, sizeof(error_resp.error_message),
                    "Unknown message type: %u", header.type);
            
            write(client_fd, &resp_header, sizeof(resp_header));
            write(client_fd, &error_resp, sizeof(error_resp));
            break;
        }
    }
    
    // Clean up
    if (payload) {
        free(payload);
    }
    
    return 0;
}

// Check if daemon is running
int daemon_is_running(void) {
    return daemon_running;
}

// Stop daemon
void daemon_stop(void) {
    daemon_running = 0;
}
