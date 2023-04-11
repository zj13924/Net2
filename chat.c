#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/times.h>
#include <unistd.h>
#define NAME_SIZE 40
#define BUF_SIZE 1024
#define BUF_SIZE_LONG 2048
#define MAX_CLIENTS 10
#define PORT_NO (u_short)20000
#define Err(x)                \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }
int i, j, msgLength, inputLength, result;
char buf[BUF_SIZE], inputBuf[BUF_SIZE], strBuf[BUF_SIZE_LONG], hostName[NAME_SIZE];
struct sockaddr_in sockAddr;
struct hostent *host;
struct timeval time;
fd_set mask;

void startServer();
void startClient();

int main()
{
    time.tv_sec = 0;
    time.tv_usec = 1;
    int role = 0;
    while (1)
    {
        printf("Please select your role.\n");
        printf("Server: 1 ; Client: 2 \n");
        scanf("%d", &role);
        printf("\n");
        if (role == 1 || role == 2)
            break;
    }
    if (role == 1)
        startServer();
    if (role == 2)
        startClient();
}

void startServer()
{
    int fdToListen, fdClientList[MAX_CLIENTS], fd, fdTemp, fdMax, clientCnt = 0;
    bool hasNext;
    bzero(fdClientList, sizeof(int) * MAX_CLIENTS);
    if (gethostname(hostName, sizeof(hostName)) < 0)
        Err("gethostname");
    printf("Your hostname is '%s'\n", hostName);
    host = gethostbyname(hostName);
    if (host == NULL)
        Err("gethostbyname");
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);
    bcopy((char *)host->h_addr,
          (char *)&sockAddr.sin_addr, host->h_length);
    fdToListen = socket(AF_INET, SOCK_STREAM, 0);
    if (fdToListen < 0)
        Err("socket");
    if (bind(fdToListen, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
        Err("bind");
    listen(fdToListen, 1);
    write(1, "Waiting for someone to connect...\n", 34);

    while (1)
    {
        FD_ZERO(&mask);
        FD_SET(fdToListen, &mask);
        FD_SET(0, &mask);

        fdMax = fdToListen;
        hasNext = true;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            fd = fdClientList[i];
            if (fd > 0)
                FD_SET(fd, &mask);
            if (fd > fdMax)
                fdMax = fd;
        }

        result = select(fdMax + 1, &mask, NULL, NULL, &time);
        if (result < 0)
            Err("select");

        // A client called connect()
        if (FD_ISSET(fdToListen, &mask))
        {
            fd = accept(fdToListen, NULL, NULL);
            if (fd < 0)
                Err("accept");
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (fdClientList[i] == 0)
                {
                    fdClientList[i] = fd;
                    clientCnt++;
                    break;
                }
            }
        }

        // Message input via stdio (fd=0 means standard input)
        inputLength = 0;
        if (FD_ISSET(0, &mask))
        {
            bzero(inputBuf, BUF_SIZE);
            inputLength = read(0, inputBuf, BUF_SIZE);
            // "Quit" command input
            if (!strcmp(inputBuf, "Q\n") || inputLength == 0)
            {
                strcpy(inputBuf, "Q\n");
                hasNext = false;
            }
        }

        // Iterate through all active clients
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            fd = fdClientList[i];
            if (fd == 0)
                continue;
            // If the input message is not empty, send it to the current client
            if (inputLength > 0)
            {
                bzero(strBuf, BUF_SIZE_LONG);
                sprintf(strBuf, "server: %s", inputBuf);
                write(fd, strBuf, strlen(strBuf));
            }
            // Message received from the current client
            if (FD_ISSET(fd, &mask))
            {
                bzero(buf, BUF_SIZE);
                msgLength = read(fd, buf, BUF_SIZE);
                // "Quit" command received
                if (!strcmp(buf, "Q\n") || msgLength == 0)
                {
                    close(fd);
                    fdClientList[i] = 0;
                    clientCnt--;
                    continue;
                }

                bzero(strBuf, BUF_SIZE_LONG);
                sprintf(strBuf, "client[%02d]: %s", i, buf);
                // Print the received message (fd=1 means standard output)
                write(1, strBuf, strlen(strBuf));
                // And also send that message to all other clients,
                // except for the client who sent the message just now
                for (j = 0; j < MAX_CLIENTS; j++)
                {
                    fdTemp = fdClientList[j];
                    if (j == i || fdTemp == 0)
                        continue;
                    write(fdTemp, strBuf, strlen(strBuf));
                }
            }
        }
        if (!hasNext)
        {
            break;
        }
    }

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        fd = fdClientList[i];
        if (fd > 0)
            close(fd);
    }
    close(fdToListen);
}

void startClient()
{
    int fdToConnect;
    printf("Please enter the server's hostname:\n");
    scanf("%s", hostName);
    host = gethostbyname(hostName);
    if (host == NULL)
        Err("gethostbyname");
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);
    bcopy((char *)host->h_addr,
          (char *)&sockAddr.sin_addr, host->h_length);
    fdToConnect = socket(AF_INET, SOCK_STREAM, 0);
    if (fdToConnect < 0)
        Err("socket");
    connect(fdToConnect, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
    write(1, "Please wait until someone sends a message...\n", 45);

    while (1)
    {
        FD_ZERO(&mask);
        FD_SET(fdToConnect, &mask);
        FD_SET(0, &mask);
        result = select(fdToConnect + 1, &mask, NULL, NULL, &time);
        if (result < 0)
            Err("select");
        // Message received from server
        if (FD_ISSET(fdToConnect, &mask))
        {
            bzero(buf, BUF_SIZE);
            msgLength = read(fdToConnect, buf, BUF_SIZE);
            // "Quit" command received
            if (!strcmp(buf, "Q\n") || msgLength == 0)
                break;
            // Print the received message (fd=1 means standard output)
            write(1, buf, msgLength);
        }
        // Message input via stdio (fd=0 means standard input)
        if (FD_ISSET(0, &mask))
        {
            bzero(inputBuf, BUF_SIZE);
            inputLength = read(0, inputBuf, BUF_SIZE);
            // Send the message to server
            write(fdToConnect, inputBuf, inputLength);
            // "Quit" command input
            if (!strcmp(inputBuf, "Q\n") || inputLength == 0)
                break;
        }
    }
    close(fdToConnect);
}
