/*
  Class 4I, 2024, NIT Ibaraki College
  Information Networks II sample code
  TCP chat program(non-blocking version with multiple connections)
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#define NAME_SIZE 256
#define BUF_SIZE 1024
#define PORT_NO (u_short)20000
#define PROTOCOL_DELIMITER "-\n"
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

struct Client
{
    int fd;
    char *name;
    struct Client *next;
};

const char *NAME = "NAME";
const char *XYZ = "XYZ"; // STUB!

char buf[BUF_SIZE], hostName[NAME_SIZE];
struct sockaddr_in sockAddr;
struct hostent *host;
int fdSock, result;
struct timeval timeVal;
fd_set fdSet;

void startServer();
void startClient(char *);
struct Client *appendClient(int, char *, struct Client *);
bool parseCommand(char *, struct Client *);

int main(int argc, char *argv[])
{
    // Set the blocking time for select()
    timeVal.tv_sec = 0;
    timeVal.tv_usec = 10;

    if (argc > 1)
        strcpy(buf, argv[1]);
    while (1)
    {
        if (strcmp(buf, "1") == 0 || strcmp(buf, "2") == 0)
            break;
        printf("Please select your role.\n");
        printf("Server: 1 ; Client: 2 \n");
        scanf("%s", buf);
    }
    if (strcmp(buf, "1") == 0)
        startServer();
    else if (strcmp(buf, "2") == 0)
    {
        if (argc > 2)
            startClient(argv[2]);
        else
            startClient(NULL);
    }
    return 0;
}

void startServer()
{
    bool hasNext = true;
    char *dynBuf;
    struct Client *clientRoot, *client;
    // Use the first element (root) of the client list to store the server's info
    clientRoot = appendClient(0, "server", NULL);

    if (gethostname(hostName, sizeof(hostName)) < 0)
        ErrorExit("gethostname");
    host = gethostbyname(hostName);
    if (host == NULL)
        ErrorExit("gethostbyname");
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);
    bcopy((char *)host->h_addr,
          (char *)&sockAddr.sin_addr, host->h_length);
    fdSock = socket(AF_INET, SOCK_STREAM, 0);
    if (fdSock < 0)
        ErrorExit("socket");
    int option = 1;
    setsockopt(fdSock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (bind(fdSock, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
        ErrorExit("bind");
    listen(fdSock, 1);

    dynBuf = (char *)calloc(INET_ADDRSTRLEN + 1, sizeof(char));
    if (inet_ntop(AF_INET, host->h_addr, dynBuf, INET_ADDRSTRLEN) == NULL)
        ErrorExit("inet_ntop");
    printf("Your hostname and IPv4 address is '%s', %s\n", hostName, dynBuf);
    printf("Waiting for someone to connect...\n");
    free(dynBuf);

    while (hasNext)
    {
        // Initialize the fd_set and attach the target FD to be supervised
        FD_ZERO(&fdSet);
        FD_SET(fdSock, &fdSet);

        // Attach the FDs of all active clients to the fd_set,
        // and find out the greatest value of FD
        int fdMax = fdSock;
        for (client = clientRoot; client != NULL; client = client->next)
        {
            if (client->fd >= 0)
                FD_SET(client->fd, &fdSet);
            if (client->fd > fdMax)
                fdMax = client->fd;
        }

        result = select(fdMax + 1, &fdSet, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // When a new client is about to join the chat (calls connect())
        if (FD_ISSET(fdSock, &fdSet))
        {
            int fd = accept(fdSock, NULL, NULL);
            if (fd < 0)
                ErrorExit("accept");

            // Find the last client within the linked list
            int clientCnt = 1;
            for (client = clientRoot; client != NULL; client = client->next)
            {
                if (client->next == NULL)
                    break;
                clientCnt++;
            }
            dynBuf = (char *)calloc(NAME_SIZE, sizeof(char));
            sprintf(dynBuf, "client_%02d", clientCnt);
            client->next = appendClient(fd, dynBuf, NULL);
            free(dynBuf);
        }

        // Iterate through all active clients
        for (client = clientRoot; client != NULL; client = client->next)
        {
            // A negative FD value means the client is disconnected
            if (client->fd < 0)
                continue;

            // When the FD of the current client/stdin is ready for read
            if (FD_ISSET(client->fd, &fdSet))
            {
                // Receive/read the message from the peer/stdin
                bzero(buf, sizeof(char) * BUF_SIZE);
                // When the incoming message is empty or contains the "Quit" command
                // An empty message means the connection has already been closed by the peer
                if (read(client->fd, buf, BUF_SIZE) == 0 || strcmp(buf, "Q\n") == 0)
                {
                    // When the message comes from a peer
                    if (client->fd > 0)
                    {
                        // Perform a "graceful" close
                        close(client->fd);
                        // Mark the curent client as disconnected
                        client->fd = -1;
                        continue;
                    }
                    // Otherwise when it comes from stdin
                    else
                        strcpy(buf, "Q\n");
                }
                else
                // The normal case when the incoming message is not something special
                {
                    // Interpret the meaning of the message before using it
                    parseCommand(buf, client);
                    // Modify the message before forwarding/printing it
                    dynBuf = strdup(buf);
                    snprintf(buf, BUF_SIZE, "%s: %s", client->name, dynBuf);
                    free(dynBuf);
                }

                // Forward the received message to all active clients,
                // except for the one who sent that message just now
                struct Client *cln;
                for (cln = clientRoot; cln != NULL; cln = cln->next)
                {
                    // Skip the same client
                    if (client->fd == cln->fd)
                        continue;
                    write(cln->fd, buf, strlen(buf));
                }

                // "Quit" command input from stdin
                if (client->fd == 0 && strcmp(buf, "Q\n") == 0)
                {
                    // Perform a "graceful" close
                    close(fdSock);
                    hasNext = false;
                    break;
                }
            }
        }
    }
    close(fdSock);
}

void startClient(char *serverName)
{
    unsigned char ip[4];
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);
    if (serverName != NULL)
        strcpy(hostName, serverName);
    else
    {
        printf("Please enter the hostname or IPv4 address to connect with:\n");
        scanf("%s", hostName);
    }
    if (sscanf(hostName, "%hhu.%hhu.%hhu.%hhu", &ip[0], &ip[1], &ip[2], &ip[3]) == 4)
    {
        char ipStr[20];
        struct in_addr serverAddr;
        sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        if (inet_aton(ipStr, &serverAddr) == 0)
            ErrorExit("inet_aton");
        bcopy((char *)&serverAddr,
              (char *)&sockAddr.sin_addr, 4);
    }
    else
    {
        host = gethostbyname(hostName);
        if (host == NULL)
            ErrorExit("gethostbyname");
        bcopy((char *)host->h_addr,
              (char *)&sockAddr.sin_addr, host->h_length);
    }

    fdSock = socket(AF_INET, SOCK_STREAM, 0);
    if (fdSock < 0)
        ErrorExit("socket");
    int option = 1;
    setsockopt(fdSock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    connect(fdSock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
    printf("Please wait until someone sends a message...\n");

    while (1)
    {
        // Initialize the FD_SET and set the FDs to supervise
        FD_ZERO(&fdSet);
        FD_SET(0, &fdSet);
        FD_SET(fdSock, &fdSet);

        result = select(fdSock + 1, &fdSet, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // When the FD of the peer is ready for read (fd = 0 means standard input)
        if (FD_ISSET(fdSock, &fdSet))
        {
            // Receive the message from the peer
            bzero(buf, sizeof(char) * BUF_SIZE);
            // Disconnect when received an empty message or the "Quit" command
            // An empty message means the connection has already been closed by the peer
            if (read(fdSock, buf, BUF_SIZE) == 0 || strcmp(buf, "Q\n") == 0)
                break;

            printf("%s", buf);
        }

        // When stdin is ready for read (fd = 0 means standard input)
        if (FD_ISSET(0, &fdSet))
        {
            bzero(buf, sizeof(char) * BUF_SIZE);
            read(0, buf, BUF_SIZE);

            // Send the message to the peer
            write(fdSock, buf, strlen(buf));

            // "Quit" command input
            if (strcmp(buf, "Q\n") == 0)
                // Perform a "graceful" close after sending the "Quit" command to the peer
                break;
        }
    }
    close(fdSock);
}

struct Client *appendClient(int fd, char *name, struct Client *next)
{
    struct Client *newClient = (struct Client *)calloc(1, sizeof(struct Client));
    newClient->fd = fd;
    newClient->name = (char *)calloc(NAME_SIZE, sizeof(char));
    newClient->next = next;
    if (name != NULL)
        strcpy(newClient->name, name);
    return newClient;
}

bool parseCommand(char *cmd, struct Client *client)
{
    bool captured = false;
    char *cmdDup = strdup(cmd);
    char *cp = strtok(cmdDup, PROTOCOL_DELIMITER);
    while (cp != NULL)
    {
        char *cmdName = cp;
        cp = strtok(NULL, PROTOCOL_DELIMITER);
        // NAME-<client name>: change the name of current client
        if (strcmp(cmdName, NAME) == 0 && cp != NULL)
        {
            strcpy(client->name, cp);
            captured = true;
            break;
        }
        // Stub branch for a new command
        else if (strcmp(cmdName, XYZ) == 0)
        {
            // STUB!
            captured = true;
            break;
        }
    }
    free(cmdDup);
    return captured;
}