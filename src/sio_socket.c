#define _GNU_SOURCE

#include <errno.h>
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
    LogMsg(LOG_ERR, "Exiting: %s\n", errorMessage);
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

    /* remove socket if exists */
    const int status = unlink(socketPath);
    if ((status != 0) && (errno != ENOENT)) {
        sioDieWithError("socket file unlink failed\n");
    }

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

    const int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr,
        &clientLength);
    if (clientFd >= 0) {
        switch (addressFamily) {
        case AF_UNIX:
            LogMsg(LOG_INFO, "Handling Unix client\n");
            break;

        case AF_INET:
            LogMsg(LOG_INFO, "Handling TCP client %s\n",
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
 * Reads a single message from the socket connected to the 
 * tio-agent. If no message is ready to be received, the call 
 * will block until one is available. 
 * 
 * @param socketFd the file descriptor of for the already open 
 *                 socket connecting to the tio-agent
 * @param msgBuff address of a contiguous array into which the 
 *                message will be written upon receipt from the
 *                tio-agent
 * @param bufferSize the number of bytes in msgBuff
 * 
 * @return int 0 if no message to return (handled here), -1 if 
 *         recv() returned an error code (close connection) or
 *         >0 to indicate msgBuff has that many characters
 *         filled in
 */
int sioTioSocketRead(int socketFd, char *msgBuff, size_t bufferSize)
{
    int cnt;

    if ((cnt = recv(socketFd, msgBuff, bufferSize, 0)) <= 0) {
        LogMsg(LOG_INFO, "%s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    } else {
        msgBuff[cnt] = 0;
        LogMsg(LOG_INFO, "%s", msgBuff);
        return cnt;
    }
}


void sioTioSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);

    if (send(socketFd, buff, cnt, 0) != cnt) {
        LogMsg(LOG_ERR, "socket_send_to_client(): send() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
}
