/*
  Class 4I, 2024, NIT Ibaraki College
  Information Networks II sample code
  TCP chat program(non-blocking version)
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
#include <unistd.h>
#define NAME_SIZE 256
#define BUF_SIZE 1024
#define PORT_NO (u_short)20000
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

char buf[BUF_SIZE], hostName[NAME_SIZE];
struct sockaddr_in sockAddr;
struct hostent *host;
int fdSock, result;
struct timeval timeVal;
fd_set fdSet;

void startServer();
void startClient(char *);

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
    int fdAcpt;
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

    char *ipBuf = (char *)calloc(INET_ADDRSTRLEN + 1, sizeof(char));
    if (inet_ntop(AF_INET, host->h_addr, ipBuf, INET_ADDRSTRLEN) == NULL)
        ErrorExit("inet_ntop");
    printf("Your hostname and IPv4 address is '%s', %s\n", hostName, ipBuf);
    printf("Waiting for someone to connect...\n");

    fdAcpt = accept(fdSock, NULL, NULL);
    if (fdAcpt < 0)
        ErrorExit("accept");

    while (1)
    {
        // Initialize the fd_set and attach the target FDs to be supervised
        FD_ZERO(&fdSet);
        FD_SET(0, &fdSet);
        FD_SET(fdAcpt, &fdSet);

        result = select(fdAcpt + 1, &fdSet, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // When stdin is ready for read (fd = 0 means standard input)
        if (FD_ISSET(0, &fdSet))
        {
            bzero(buf, sizeof(char) * BUF_SIZE);
            read(0, buf, BUF_SIZE);

            // Send the message to the peer
            write(fdAcpt, buf, strlen(buf));

            // "Quit" command input
            if (strcmp(buf, "Q\n") == 0)
            {
                // Perform a "graceful" close after sending the "Quit" command to the peer
                close(fdAcpt);
                break;
            }
        }

        // When the FD of the peer is ready for read
        if (FD_ISSET(fdAcpt, &fdSet))
        {
            // Receive the message from the peer
            bzero(buf, sizeof(char) * BUF_SIZE);
            // Disconnect when received an empty message or the "Quit" command
            // An empty message means the connection has already been closed by the peer
            if (read(fdAcpt, buf, BUF_SIZE) == 0 || strcmp(buf, "Q\n") == 0)
            {
                close(fdAcpt);
                break;
            }
            printf("client: %s", buf);
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
        // Initialize the fd_set and attach the target FDs to be supervised
        FD_ZERO(&fdSet);
        FD_SET(0, &fdSet);
        FD_SET(fdSock, &fdSet);

        result = select(fdSock + 1, &fdSet, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // When the FD of the peer is ready for read
        if (FD_ISSET(fdSock, &fdSet))
        {
            // Receive the message from the peer
            bzero(buf, sizeof(char) * BUF_SIZE);
            // Disconnect when received an empty message or the "Quit" command
            // An empty message means the connection has already been closed by the peer
            if (read(fdSock, buf, BUF_SIZE) == 0 || strcmp(buf, "Q\n") == 0)
                break;

            printf("server: %s", buf);
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
