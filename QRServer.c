#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

#define LOG_FILE "admin_log.txt"

// Server configuration constants
#define DEFAULT_PORT 3600 // Default port number
#define DEFAULT_RATE_LIMIT 2
#define DEFAULT_RATE_WINDOW 60
#define DEFAULT_MAX_USERS 3
#define DEFAULT_TIMEOUT 90
#define MAX_IMAGE_SIZE (10 * 1024 * 1024)  // 10 MB (original maximum)
#define SECURE_MAX_IMAGE_SIZE (100 * 1024) // Enforced maximum file size (in bytes)

// Response codes for client
#define CODE_SUCCESS 0
#define CODE_FAILURE 1
#define CODE_TIMEOUT 2
#define CODE_RATE_LIMIT 3

// Server configuration structure
typedef struct
{
    int port;
    int rate_limit;
    int rate_window;
    int max_users;
    int timeout;
} ServerConfig;

ServerConfig config = {
    .port = DEFAULT_PORT,
    .rate_limit = DEFAULT_RATE_LIMIT,
    .rate_window = DEFAULT_RATE_WINDOW,
    .max_users = DEFAULT_MAX_USERS,
    .timeout = DEFAULT_TIMEOUT};

volatile sig_atomic_t shutdown_requested = 0;
int user_count = 0;

// Signal handler for shutdown (SIGUSR1)
void handle_shutdown_signal(int sig)
{
    shutdown_requested = 1;
}

// Log events with timestamp & client IP 
void log_event(const char *ip, const char *format, ...)
{
    FILE *logf = fopen(LOG_FILE, "a");
    if (!logf)
        return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char ip_part[INET_ADDRSTRLEN] = "SERVER";
    if (ip != NULL)
    {
        strncpy(ip_part, ip, INET_ADDRSTRLEN - 1);
        ip_part[INET_ADDRSTRLEN - 1] = '\0';
    }

    fprintf(logf, "%04d-%02d-%02d %02d:%02d:%02d %s ",
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ip_part);

    va_list args;
    va_start(args, format);
    vfprintf(logf, format, args);
    va_end(args);

    fprintf(logf, "\n");
    fclose(logf);
}

// Parse command-line arguments to update server configuration.
void parse_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-PORT") == 0) && i + 1 < argc)
        {
            config.port = atoi(argv[++i]);
            log_event(NULL, "Config updated: PORT = %d", config.port);
        }
        else if ((strcmp(argv[i], "-RATE_MSGS") == 0 || strcmp(argv[i], "-RATE_MSGS") == 0) && i + 1 < argc)
        {
            config.rate_limit = atoi(argv[++i]);
            log_event(NULL, "Config updated: RATE_MSGS = %d", config.rate_limit);
        }
        else if ((strcmp(argv[i], "-RATE_TIME") == 0 || strcmp(argv[i], "-RATE_TIME") == 0) && i + 1 < argc)
        {
            config.rate_window = atoi(argv[++i]);
            log_event(NULL, "Config updated: RATE_TIME = %d", config.rate_window);
        }
        else if ((strcmp(argv[i], "-MAX_USERS") == 0 || strcmp(argv[i], "-MAX_USERS") == 0) && i + 1 < argc)
        {
            config.max_users = atoi(argv[++i]);
            log_event(NULL, "Config updated: MAX_USERS = %d", config.max_users);
        }
        else if ((strcmp(argv[i], "-TIME_OUT") == 0 || strcmp(argv[i], "-TIME_OUT") == 0) && i + 1 < argc)
        {
            config.timeout = atoi(argv[++i]);
            log_event(NULL, "Config updated: TIME_OUT = %d", config.timeout);
        }
        else
        {
            fprintf(stderr, "Unknown or invalid argument: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }
}

// Call Java ZXing decoder to read QR from image file given its path.
int decode_qr_image(const char *image_path, char *decoded_output, size_t output_size)
{
    char command[1024];
#ifdef _WIN32
    const char *sep = ";";
#else
    const char *sep = ":";
#endif

    snprintf(command, sizeof(command),
             "java -cp javase.jar%score.jar com.google.zxing.client.j2se.CommandLineRunner \"%s\" 2>/dev/null",
             sep, image_path);

    FILE *process = popen(command, "r");
    if (!process)
    {
        snprintf(decoded_output, output_size, "Failed to decode QR code");
        return -1;
    }

    char buffer[512];
    int found = 0;

    while (fgets(buffer, sizeof(buffer), process))
    {
        if (strstr(buffer, "Parsed result:") != NULL)
        {
            // Next line is the decoded text.
            if (fgets(buffer, sizeof(buffer), process))
            {
                strncpy(decoded_output, buffer, output_size - 1);
                decoded_output[strcspn(decoded_output, "\r\n")] = '\0'; // Trim newline.
                found = 1;
                break;
            }
        }
    }

    pclose(process);
    return found ? 0 : -1;
}

// Client connection handling function
void handle_client(int client_fd, const char *ip)
{
    log_event(ip, "Handling client connection");
    printf("Client [%s]: Handling connection.\n", ip);

    // Set receive timeout on client's socket.
    struct timeval tv;
    tv.tv_sec = config.timeout;
    tv.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
    {
        perror("setsockopt failed");
    }

    // Rate limiter state for this client.
    time_t rate_window_start = time(NULL);
    int request_count = 0;

    char command[256];
    ssize_t bytes_read = 0;

    // Main command loop.
    while (1)
    {
        memset(command, 0, sizeof(command));
        bytes_read = recv(client_fd, command, sizeof(command) - 1, 0);
        if (bytes_read < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                uint32_t net_code = htonl(CODE_TIMEOUT);
                send(client_fd, &net_code, sizeof(net_code), 0);
                printf("Client [%s]: Timeout. Sending TIMEOUT (%d).\n", ip, CODE_TIMEOUT);
                log_event(ip, "Client timed out. Sending TIMEOUT (%d).", CODE_TIMEOUT);
                break;
            }
            perror("recv error");
            break;
        }
        else if (bytes_read == 0)
        {
            log_event(ip, "Client disconnected (EOF).");
            printf("Client [%s]: Disconnected (EOF).\n", ip);
            break;
        }

        command[strcspn(command, "\r\n")] = '\0'; // Remove newlines.
        log_event(ip, "Received command: %s", command);
        printf("Client [%s]: Received command: %s\n", ip, command);

        if (strcmp(command, "close") == 0)
        {
            log_event(ip, "Client requested disconnect.");
            printf("Client [%s]: Requested disconnect.\n", ip);
            break;
        }
        if (strcmp(command, "shutdown") == 0)
        {
            log_event(ip, "Shutdown command received from client.");
            printf("Client [%s]: Shutdown command received. Signaling server shutdown.\n", ip);
            kill(getppid(), SIGUSR1); // Signal parent process to shutdown.
            break;
        }

        // --- Rate Limiting ---
        time_t now = time(NULL);
        if (difftime(now, rate_window_start) >= config.rate_window)
        {
            rate_window_start = now; // Reset time window
            request_count = 0;       // Reset request count
        }
        request_count++; // Increment the per-client request count

        if (request_count >= config.rate_limit)
        { // Enforce correct rate limit
            uint32_t net_code = htonl(CODE_RATE_LIMIT);
            const char *rate_msg = "Rate limit exceeded. Please wait before sending another request.";
            uint32_t net_msg_len = htonl(strlen(rate_msg));

            send(client_fd, &net_code, sizeof(net_code), 0);
            send(client_fd, &net_msg_len, sizeof(net_msg_len), 0);
            send(client_fd, rate_msg, strlen(rate_msg), 0);

            log_event(ip, "Rate limit exceeded for client [%s] -> RATE_LIMIT (%d)", ip, CODE_RATE_LIMIT);
            printf("Client [%s]: Rate limit exceeded. Sending RATE_LIMIT (%d).\n", ip, CODE_RATE_LIMIT);

            continue; // Prevent further processing
        }
        // --- End Rate Limiting ---

        // Assume command is a PNG filename.
        struct stat st;
        if (stat(command, &st) != 0)
        {
            uint32_t net_code = htonl(CODE_FAILURE);
            uint32_t net_len = htonl(0);
            send(client_fd, &net_code, sizeof(net_code), 0);
            send(client_fd, &net_len, sizeof(net_len), 0);
            log_event(ip, "File %s not found. Sending FAILURE (%d).", command, CODE_FAILURE);
            printf("Client [%s]: File %s not found. Sending FAILURE (%d).\n", ip, command, CODE_FAILURE);
            continue;
        }
        if (st.st_size > SECURE_MAX_IMAGE_SIZE)
        {
            uint32_t net_code = htonl(CODE_FAILURE);
            uint32_t net_len = htonl(0);
            send(client_fd, &net_code, sizeof(net_code), 0);
            send(client_fd, &net_len, sizeof(net_len), 0);
            log_event(ip, "File %s exceeds allowed size (%ld bytes). Sending FAILURE (%d).", command, st.st_size, CODE_FAILURE);
            printf("Client [%s]: File %s exceeds allowed size (%ld bytes). Sending FAILURE (%d).\n", ip, command, st.st_size, CODE_FAILURE);
            continue;
        }

        // Attempt to decode the QR code.
        char decoded_output[1024] = {0};
        int decode_status = decode_qr_image(command, decoded_output, sizeof(decoded_output));
        int code = (decode_status == 0) ? CODE_SUCCESS : CODE_FAILURE;
        uint32_t net_code = htonl(code);
        uint32_t net_len = htonl(strlen(decoded_output));

        send(client_fd, &net_code, sizeof(net_code), 0);
        send(client_fd, &net_len, sizeof(net_len), 0);
        if (strlen(decoded_output) > 0)
            send(client_fd, decoded_output, strlen(decoded_output), 0);

        // Log and print the result along with the status code.
        if (code == CODE_SUCCESS)
        {
            log_event(ip, "QR decode success for file %s: SUCCESS (%d): %s", command, code, decoded_output);
            printf("Client [%s]: Decoded file %s successfully. Sending SUCCESS (%d): %s\n", ip, command, code, decoded_output);
        }
        else
        {
            log_event(ip, "QR decode failure for file %s: FAILURE (%d)", command, code);
            printf("Client [%s]: Failed to decode file %s. Sending FAILURE (%d).\n", ip, command, code);
        }
    }

    close(client_fd);
    log_event(ip, "Client connection closed.");
    printf("Client [%s]: Connection closed.\n", ip);
}

// Reap any child processes to prevent zombies.
void reap_zombies()
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        if (user_count > 0)
            user_count--;
    }
}

int main(int argc, char *argv[])
{
    // Set up the shutdown signal handler.
    signal(SIGUSR1, handle_shutdown_signal);

    parse_arguments(argc, argv);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket.
    if (server_fd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, config.max_users) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("QR Server started on port %d\n", config.port);
    printf("Max users: %d | Rate limit: %d msgs / %d sec | Timeout: %d sec\n",
           config.max_users, config.rate_limit, config.rate_window, config.timeout);
    log_event("SERVER", "Server started on port %d", config.port);

    // Main accept loop.
    while (!shutdown_requested)
    {
        reap_zombies();
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("Accept failed");
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        if (user_count >= config.max_users)
        {
            const char *busy_msg = "Server busy. Try again later.\n";
            send(client_fd, busy_msg, strlen(busy_msg), 0);
            close(client_fd);
            log_event(ip_str, "Connection refused: max users reached");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Fork failed");
            close(client_fd);
            continue;
        }
        else if (pid == 0)
        {
            // Child process.
            close(server_fd);
            log_event(ip_str, "Client connected");
            handle_client(client_fd, ip_str);
            log_event(ip_str, "Client disconnected");
            exit(0);
        }
        else
        {
            // Parent process.
            close(client_fd);
            user_count++;
        }
    }

    close(server_fd);
    log_event("SERVER", "Server shutting down");
    printf("Server shutting down...\n");
    return 0;
}
