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

#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>

#define NAME_SIZE 40
#define BUF_SIZE 1024
#define BUF_SIZE_LONG 2048
#define MAX_CLIENTS 10
#define PORT_NO (u_short)20000
#define ErrorExit(x)          \
    {                         \
        fprintf(stderr, "-"); \
        perror(x);            \
        exit(0);              \
    }

struct Participant
{
    char *name;
    char *ip;
    int fd;
    unsigned long color;
    bool active;
    struct Participant *next;
};

int i, j, msgLength, inputLength, result;
char buf[BUF_SIZE], inputBuf[BUF_SIZE], strBuf[BUF_SIZE_LONG], hostName[NAME_SIZE];
struct sockaddr_in sockAddr;
struct hostent *host;
struct timeval timeVal;
fd_set mask;

void startServer();
void startClient();
char *inferSubnetIP();

int main()
{
    timeVal.tv_sec = 0;
    timeVal.tv_usec = 1;
    while (1)
    {
        printf("Please select your role.\n");
        printf("Server: 1 ; Client: 2 \n");
        scanf("%s", inputBuf);
        if (strcmp(inputBuf, "1") == 0 || strcmp(inputBuf, "2") == 0)
            break;
    }
    if (strcmp(inputBuf, "1") == 0)
        startServer();
    else if (strcmp(inputBuf, "2") == 0)
        startClient();
}

void startServer()
{
    int fdToListen, fdClientList[MAX_CLIENTS], fd, fdTemp, fdMax, clientCnt = 0;
    bool hasNext;
    bzero(fdClientList, sizeof(int) * MAX_CLIENTS);

    fdToListen = socket(AF_INET, SOCK_STREAM, 0);
    if (fdToListen < 0)
        ErrorExit("socket");
    int option = 1;
    setsockopt(fdToListen, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    char *ipBuf = inferSubnetIP();
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);

    if (strlen(ipBuf) < 1)
    {
        if (gethostname(hostName, sizeof(hostName)) < 0)
            ErrorExit("gethostname");
        host = gethostbyname(hostName);
        if (host == NULL)
            ErrorExit("gethostbyname");
        bcopy((char *)host->h_addr, (char *)&sockAddr.sin_addr, host->h_length);
        if (inet_ntop(AF_INET, host->h_addr, ipBuf, INET_ADDRSTRLEN) == NULL)
            ErrorExit("inet_ntop");
        printf("Your hostname and IPv4 address is '%s', %s\n", hostName, ipBuf);
    }
    else
    {
        struct in_addr serverAddr;
        if (inet_aton(ipBuf, &serverAddr) == 0)
            ErrorExit("inet_aton");
        bcopy((char *)&serverAddr,
              (char *)&sockAddr.sin_addr, 4);
        printf("Your subnet IPv4 address seems to be %s\n", ipBuf);
    }
    printf("Waiting for someone to connect...\n");

    if (bind(fdToListen, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
        ErrorExit("bind");
    listen(fdToListen, 1);

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

        result = select(fdMax + 1, &mask, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");

        // A new client called connect()
        if (FD_ISSET(fdToListen, &mask))
        {
            fd = accept(fdToListen, NULL, NULL);
            if (fd < 0)
                ErrorExit("accept");
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
            bzero(inputBuf, sizeof(char) * BUF_SIZE);
            inputLength = read(0, inputBuf, BUF_SIZE);
            // "Quit" command input
            if (strcmp(inputBuf, "Q\n") == 0 || inputLength == 0)
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
                bzero(strBuf, sizeof(char) * BUF_SIZE_LONG);
                if (strcmp(inputBuf, "Q\n") == 0)
                    strcpy(strBuf, inputBuf);
                else
                    sprintf(strBuf, "server: %s", inputBuf);
                write(fd, strBuf, strlen(strBuf));
            }
            // Message received from the current client
            if (FD_ISSET(fd, &mask))
            {
                bzero(buf, sizeof(char) * BUF_SIZE);
                msgLength = read(fd, buf, BUF_SIZE);
                // "Quit" command received
                if (strcmp(buf, "Q\n") == 0 || msgLength == 0)
                {
                    close(fd);
                    fdClientList[i] = 0;
                    clientCnt--;
                    continue;
                }

                bzero(strBuf, sizeof(char) * BUF_SIZE_LONG);
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
            break;
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
    unsigned char ip[4];
    bzero((char *)&sockAddr, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(PORT_NO);

    printf("Please enter the server's hostname or IPv4 address:\n");
    scanf("%s", hostName);
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

    fdToConnect = socket(AF_INET, SOCK_STREAM, 0);
    if (fdToConnect < 0)
        ErrorExit("socket");
    int option = 1;
    setsockopt(fdToConnect, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    connect(fdToConnect, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
    write(1, "Please wait until someone sends a message...\n", 45);

    while (1)
    {
        FD_ZERO(&mask);
        FD_SET(fdToConnect, &mask);
        FD_SET(0, &mask);
        result = select(fdToConnect + 1, &mask, NULL, NULL, &timeVal);
        if (result < 0)
            ErrorExit("select");
        // Message received from server
        if (FD_ISSET(fdToConnect, &mask))
        {
            bzero(buf, sizeof(char) * BUF_SIZE);
            msgLength = read(fdToConnect, buf, BUF_SIZE);
            // "Quit" command received
            if (strcmp(buf, "Q\n") == 0 || msgLength == 0)
                break;
            // Print the received message (fd=1 means standard output)
            write(1, buf, msgLength);
        }
        // Message input via stdio (fd=0 means standard input)
        if (FD_ISSET(0, &mask))
        {
            bzero(inputBuf, sizeof(char) * BUF_SIZE);
            inputLength = read(0, inputBuf, BUF_SIZE);
            // Send the message to server
            write(fdToConnect, inputBuf, inputLength);
            // "Quit" command input
            if (strcmp(inputBuf, "Q\n") == 0 || inputLength == 0)
                break;
        }
    }
    close(fdToConnect);
}

char *inferSubnetIP()
{
    // https://www.ibm.com/docs/en/i/7.1?topic=ssw_ibm_i_71/apis/getifaddrs.htm
    struct ifaddrs *interfaceArray = NULL, *tempIfAddr = NULL;
    char addressOutputBuffer[INET_ADDRSTRLEN + 1];
    char *result = (char *)calloc(INET_ADDRSTRLEN + 1, sizeof(char));
    if (getifaddrs(&interfaceArray) == 0)
    {
        for (tempIfAddr = interfaceArray; tempIfAddr != NULL; tempIfAddr = tempIfAddr->ifa_next)
        {
            if (tempIfAddr->ifa_addr->sa_family == AF_INET) // IPv4 only
            {
                struct in_addr *tempAddrPtr =
                    &((struct sockaddr_in *)tempIfAddr->ifa_addr)->sin_addr;
                // convert into the network byte orrder
                in_addr_t ipAddr = htonl(tempAddrPtr->s_addr);
                unsigned char ipMsb = ipAddr >> 24;
                const char *ipStr = inet_ntop(
                    AF_INET, tempAddrPtr, addressOutputBuffer, sizeof(addressOutputBuffer));
                if ((ipMsb == 0 || ipMsb == 127 || ipStr == NULL))
                    continue;
                strcpy(result, ipStr);
            }
        }
        freeifaddrs(interfaceArray); /* free the dynamic memory */
        interfaceArray = NULL;       /* prevent use after free  */
    }
    else
        printf("getifaddrs() failed with errno =  %d %s \n", errno, strerror(errno));
    return result;
}

