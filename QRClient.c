#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>

#define DEFAULT_PORT 3600
#define DEFAULT_SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

// Response codes matching those on the server
#define CODE_SUCCESS 0
#define CODE_FAILURE 1
#define CODE_TIMEOUT 2
#define CODE_RATE_LIMIT 3

int main(int argc, char *argv[])
{
    // Determine server IP and port from command-line arguments.
    char *server_ip = DEFAULT_SERVER_IP;
    int port = DEFAULT_PORT;

    if (argc >= 2)
    {
        server_ip = argv[1];
    }

    if (argc >= 3)
    {
        port = atoi(argv[2]);
        if (port <= 0)
        {
            fprintf(stderr, "Invalid port number. Using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }

    // Create a TCP socket.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address using provided IP and port.
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server.
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection to server failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server %s on port %d\n", server_ip, port);

    char input[256];
    char response_buffer[BUFFER_SIZE];

    // Communication loop.
    while (1)
    {
        printf("Enter 'close' to disconnect, 'shutdown' to turn off server, or a QR code filename: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("Input error or EOF encountered. Disconnecting.\n");
            break;
        }

        // Remove newline characters.
        input[strcspn(input, "\r\n")] = '\0';
        if (strlen(input) == 0)
            continue; // Skip if nothing was entered.

        // Send the command to the server.
        size_t input_len = strlen(input);
        if (send(sock_fd, input, input_len, 0) != input_len)
        {
            perror("Failed to send message");
            break;
        }

        // For "close" or "shutdown", disconnect immediately.
        if (strcmp(input, "close") == 0 || strcmp(input, "shutdown") == 0)
        {
            printf("Disconnecting from server...\n");
            break;
        }

        // Now, wait for the server's response.
        // Expecting first a 32-bit response code and then a 32-bit message length.
        uint32_t net_code, net_msg_len;
        ssize_t bytes_read = recv(sock_fd, &net_code, sizeof(net_code), 0);
        if (bytes_read <= 0)
        {
            perror("Error or disconnect while receiving response code");
            break;
        }

        bytes_read = recv(sock_fd, &net_msg_len, sizeof(net_msg_len), 0);
        if (bytes_read <= 0)
        {
            perror("Error or disconnect while receiving message length");
            break;
        }

        int code = ntohl(net_code);
        int msg_len = ntohl(net_msg_len);
        memset(response_buffer, 0, sizeof(response_buffer));

        // Read the textual message if provided.
        if (msg_len > 0)
        {
            int total_received = 0;
            while (total_received < msg_len)
            {
                bytes_read = recv(sock_fd, response_buffer + total_received, msg_len - total_received, 0);
                if (bytes_read <= 0)
                {
                    perror("Error receiving complete message");
                    break;
                }
                total_received += bytes_read;
            }
            if (total_received < BUFFER_SIZE)
                response_buffer[total_received] = '\0';
            else
                response_buffer[BUFFER_SIZE - 1] = '\0';
        }

        // Process and print the response.
        if (code == CODE_SUCCESS)
        {
            printf("Server Response:\nSUCCESS (%d): %s\n", code, response_buffer);
        }
        else if (code == CODE_FAILURE)
        {
            printf("Server Response:\nFAILURE (%d): Failed to decode QR Code or file error.\n", code);
        }
        else if (code == CODE_RATE_LIMIT)
        {
            printf("Server Resconse:\nRATE_LIMIT (%d): Rate limit exceeded: %s\n", code, response_buffer);
        }
        else if (code == CODE_TIMEOUT)
        {
            printf("Server Resconse:\nTIMEOUT (%d): Server timed out your connection.\n", code);
        }
        else
        {
            printf("Received unknown response code: %d\n", code);
        }
    }

    close(sock_fd);
    printf("Connection closed.\n");
    return 0;
}
