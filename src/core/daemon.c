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
static void* handle_client(void* arg);
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

// Process a single message from client (stub for now)
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
    
    // TODO: Implement message processing in the next step
    // For now, just send back an error
    struct message_header response_header = {
        .type = MSG_ERROR,
        .payload_size = sizeof(struct error_response),
        .sequence_id = header.sequence_id,
        .reserved = 0
    };
    
    struct error_response error_resp = {
        .error_code = -1
    };
    strncpy(error_resp.error_message, "Not implemented yet", sizeof(error_resp.error_message) - 1);
    
    write(client_fd, &response_header, sizeof(response_header));
    write(client_fd, &error_resp, sizeof(error_resp));
    
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