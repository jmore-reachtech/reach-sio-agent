#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h> 
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "sio_agent.h"

#define MAXPENDING 1

static void sioDieWithError(char *errorMessage)
{
    printf("Exiting: %s\n", errorMessage);
    exit(1);
}

static int sioCreateUnixServerSocket(const char *socketPath)
{
    int sock;
    struct sockaddr_un echoServAddr;

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        sioDieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sun_family = AF_UNIX; 
    strncpy(echoServAddr.sun_path, socketPath, sizeof(echoServAddr.sun_path));
    echoServAddr.sun_path[sizeof(echoServAddr.sun_path) - 1] = '\0';

    if (bind(sock, (struct sockaddr *)&echoServAddr,
        sizeof(echoServAddr)) < 0) {
        sioDieWithError("bind() failed");
    }

    if (listen(sock, MAXPENDING) < 0) {
        sioDieWithError("listen() failed");
    }

    return sock;
}

static int sioCreateTCPServerSocket(unsigned short port)
{
    int sock;
    struct sockaddr_in echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        sioDieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET; 
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&echoServAddr,
        sizeof(echoServAddr)) < 0) {
        sioDieWithError("bind() failed");
    }

    if (listen(sock, MAXPENDING) < 0) {
        sioDieWithError("listen() failed");
    }

    return sock;
}


int sioTioSocketAccept(int serverFd, int addressFamily)
{
    /* define a variable to hold the client's address for either family */
    union {
        struct sockaddr_un unixClientAddr;
        struct sockaddr_in inetClientAddr;
    } clientAddr;
    socklen_t clientLength = sizeof(clientAddr);

    const int clientFd = accept4(serverFd, (struct sockaddr *)&clientAddr,
        &clientLength, SOCK_NONBLOCK);
    if (sioVerboseFlag && (clientFd >= 0)) {
        switch (addressFamily) {
        case AF_UNIX:
            printf("Handling Unix client\n");
            break;

        case AF_INET:
            printf("Handling TCP client %s\n",
                inet_ntoa(clientAddr.inetClientAddr.sin_addr));
            break;

        default:
            break;
        }
    }

    return clientFd;
}


int sioTioSocketInit(unsigned short port, int *addressFamily,
    const char *unixSocketPath)
{
    int listenFd = -1;

    if (port == 0) {
        /* create a Unix domain socket */
        listenFd = sioCreateUnixServerSocket(unixSocketPath);
        *addressFamily = AF_UNIX;
    } else {
        listenFd = sioCreateTCPServerSocket(port);
        *addressFamily = AF_INET;
    }
    return listenFd;
}


/**
 * 
 * 
 * @param newFd 
 * @param msgBuff 
 * @param bufferSize 
 * 
 * @return int 0 if no message to return (handled here), -1 if 
 *         recv() returned an error code (close connection) or
 *         >0 to indicate msgBuff has that many characters
 *         filled in
 */
int sioTioSocketRead(int newFd, char *msgBuff, size_t bufferSize)
{
    int cnt;

    if ((cnt = recv(newFd, msgBuff, bufferSize, 0)) <= 0) {
        printf("sioHandleServer(): recv() failed, client closed\n");
        close(newFd);
        return -1;
    } else {
        msgBuff[cnt] = 0;
        if (sioVerboseFlag) {
            printf("%s", msgBuff);
        }

	return cnt;
    }
}


void sioTioSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);

    if (send(socketFd, buff, cnt, 0) != cnt) {
        printf("socket_send_to_client(): send() failed, %d\n", socketFd);
        perror("what's messed up?");
    }
}
