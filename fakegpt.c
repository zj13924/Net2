/*
  Class 4I, 2026, NIT Ibaraki College
  Computer Networks II sample code
  HTTP-based Chat API Client.
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Network configuration
#define TARGET_HOST "api.islaytouch.com"
#define TARGET_PORT 80
#define TARGET_PATH "/chat.php"

#define PROXY_HOST "po.cc.ibaraki-ct.ac.jp"
#define PROXY_PORT 3128

void urlEncode(const char *src, char *dest);
void sendMessage(const char *message, bool useProxy);

// Simple function to encode spaces into "%20" for URL parameters
void urlEncode(const char *src, char *dest)
{
    while (*src)
    {
        if (*src == ' ')
        {
            strcpy(dest, "%20");
            dest += 3;
        }
        else if (*src == '\n')
        {
            // Ignore newline characters from fgets
        }
        else
        {
            *dest++ = *src;
        }
        src++;
    }
    *dest = '\0';
}

// Function to send a message to the Chat API and print the response
void sendMessage(const char *message, bool useProxy)
{
    int sock;
    struct sockaddr_in serverAddr;
    struct hostent *host;
    char request[2048];
    char response[4096];
    char encodedMsg[1024] = {0};

    // Encode the user message
    urlEncode(message, encodedMsg);

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Socket creation failed");
        return;
    }

    // Determine connection target based on proxy flag
    const char *connectHost = useProxy ? PROXY_HOST : TARGET_HOST;
    int connectPort = useProxy ? PROXY_PORT : TARGET_PORT;

    // Resolve hostname
    host = gethostbyname(connectHost);
    if (host == NULL)
    {
        perror("Failed to resolve hostname");
        close(sock);
        return;
    }

    // Set up server address structure
    bzero((char *)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(connectPort);
    bcopy((char *)host->h_addr, (char *)&serverAddr.sin_addr, host->h_length);

    // Connect to the target (proxy or direct)
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        return;
    }

    // Build the HTTP GET request
    if (useProxy)
    {
        // Proxy servers require the absolute URL in the request line
        sprintf(request,
                "GET http://%s%s?msg=%s HTTP/1.0\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n\r\n",
                TARGET_HOST, TARGET_PATH, encodedMsg, TARGET_HOST);
    }
    else
    {
        // Direct connections only need the relative path
        sprintf(request,
                "GET %s?msg=%s HTTP/1.0\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n\r\n",
                TARGET_PATH, encodedMsg, TARGET_HOST);
    }

    // Send the request
    write(sock, request, strlen(request));

    // Read the response from the server
    int totalRead = 0, n;
    bzero(response, sizeof(response));
    while ((n = read(sock, response + totalRead, sizeof(response) - totalRead - 1)) > 0)
    {
        totalRead += n;
    }
    close(sock);

    // Extract the JSON body from the HTTP response
    char *body = strstr(response, "\r\n\r\n");
    if (body != NULL)
    {
        body += 4; // Skip the empty line

        // Simple string parsing to extract the message from {"msg":"..."}
        char *msgStart = strstr(body, "\"msg\":\"");
        if (msgStart != NULL)
        {
            msgStart += 7; // Skip the tag
            char *msgEnd = strchr(msgStart, '\"');
            if (msgEnd != NULL)
            {
                *msgEnd = '\0'; // Null-terminate the string here
                printf("FakeGPT: %s\n", msgStart);
                return;
            }
        }

        // Fallback: print the raw body if parsing fails
        printf("Raw Response: %s\n", body);
    }
    else
    {
        printf("Error: Invalid HTTP response from server.\n");
    }
}

int main(int argc, char *argv[])
{
    char input[512];
    bool useProxy = false;

    // Parse command line arguments for the '-p' flag
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            useProxy = true;
        }
    }

    printf("========================\n");
    printf("|  FakeGPT CLI Client  |\n");
    if (useProxy)
    {
        printf("|  Mode: Proxy Active  |\n");
    }
    else
    {
        printf("|  Mode: Direct Conn.  |\n");
    }
    printf("|  Type 'Q' to close.  |\n");
    printf("========================\n\n");

    while (1)
    {
        printf("You: ");
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        // Remove trailing newline if present
        input[strcspn(input, "\n")] = 0;

        // Exit condition
        if (strcmp(input, "Q") == 0)
        {
            break;
        }

        // Skip empty inputs
        if (strlen(input) == 0)
        {
            continue;
        }

        // Send to API
        sendMessage(input, useProxy);
    }

    return 0;
}