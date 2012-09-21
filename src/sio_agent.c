#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sio_agent.h"

int verboseFlag;
int localEchoFlag;
int keepGoing;
const char *progName;

static void sioDumpHelp();
static void sioAgent(const char *serialName, unsigned short tcpPort,
    const char *unixSocketPath);
static inline int max(int a, int b) { return (a > b) ? a : b; }

int main(int argc, char *argv[])
{
    int c;
    char *sioTTYVar = 0;
    int daemonFlag = 0;

    progName = argv[0];

    verboseFlag = 0;
    localEchoFlag = 0;
    char *serialName = SIO_DEFAULT_SERIAL_DEVICE;
    unsigned short tcpPort = 0;

    /* override the TTY from environment if variable is set */
    sioTTYVar = getenv("SIO_AGENT_TTY");
    if (sioTTYVar) {
        serialName = sioTTYVar;
        fprintf(stderr, "sio_agent: Setting device to %s from SIO_AGENT_TTY\n",
            serialName);
    }

    while (1) {
        static struct option longOptions[] = {
            { "daemon",     no_argument,       0, 'd' },
            { "local_echo", no_argument,       0, 'l' },
            { "pty",        no_argument,       0, 'p' },
            { "serial",     required_argument, 0, 't' },
            { "tcp",        optional_argument, 0, 'i' },
            { "help",       no_argument,       0, 'h' },
            { "verbose",    no_argument,       0, 'v' },
            { 0,            0, 0,  0  }
        };
        int c = getopt_long(argc, argv, "di::lpt:uvh?", longOptions, 0);

        if (c == -1) {
            break;  // no more options to process
        }

        switch (c) {
        case 'd':
            daemonFlag = 1;
            break;

        case 'i':
            tcpPort = (optarg == 0) ? SIO_DEFAULT_AGENT_PORT : atoi(optarg);
            break;

        case 'l':
            localEchoFlag = 1;
            break;

        case 'p':
            serialName = 0;
            break;

        case 't':
            serialName = optarg;
            break;

        case 'u':
            tcpPort = 0;
            break;

        case 'v':
            verboseFlag = 1;
            break;

        case '?':
        case 'h':
        default:
            sioDumpHelp();
            exit(1);
        }
    }

    /*  Keep STDIO going for now.
     */
    if (daemonFlag) {
        daemon(0, 1);
    }

    sioAgent(serialName, tcpPort, SIO_AGENT_UNIX_SOCKET);

    return 0;
}

static void sioDumpHelp()
{
    fprintf(stderr, "usage: %s [options]\n"
        "    where options are:\n"
        "        -d          | --daemon        run in background\n"
        "        -i [<port>] | --tcp=[<port>]  use TCP socket, default = %d\n"
        "        -l          | --local_echo    local echo on\n"
        "        -p          | --pty           use pty device instead of real serial\n"
        "        -t          | --serial <dev>  use <dev> instead of /dev/ttyUSB0\n"
        "        -v          | --verbose       print progress messages\n"
        "        -h          | -?|--help       print usage information\n",
        progName, SIO_DEFAULT_AGENT_PORT);
}

static void sioInterruptHandler(int sig)
{
    printf("%s called\n", __FUNCTION__);
    keepGoing = 0;
}

/**
 * This is the main loop function.  It opens and configures the 
 * serial port (or pty) and opens the socket (TCP or Unix 
 * domain) and enters a select loop waiting for connections. 
 * 
 * @param serialName the name of the serial device to open or 0 
 *                   to use a pty
 * @param tcpPort if non-zero, the port number to open for 
 *                accepting connections from the tio-agent; if
 *                zero, uses a hard-coded named Unix domain
 *                socket
 */
static void sioAgent(const char *serialName, unsigned short tcpPort,
    const char *unixSocketPath)
{
    fd_set currFdSet;
    int connectedFd = -1;  /* not currently connected */

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = sioInterruptHandler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            fprintf(stderr, "sigaction() failed, errno = %d\n", errno);
            exit(1);
        }
    }

    /* open the server socket */
    int addressFamily = 0;
    const int listenFd = sioSocketInit(tcpPort, &addressFamily, unixSocketPath);
    if (listenFd < 0) {
        /* open failed, can't continue */
        fprintf(stderr, "could not open server socket\n");
        return;
    }

    FD_ZERO(&currFdSet);
    FD_SET(listenFd, &currFdSet);

    /* execution remains in this loop until a fatal error or SIGINT */
    keepGoing = 1;
    while (keepGoing) {
        int nfds = 0;
        off_t serialPos = 0;

        /* try opening the serial device */
        const int serialFd = sioTTYInit(serialName);
        if (serialFd < 0) {
            /* open failed, can't continue */
            fprintf(stderr, "could not open serial port %s\n", serialName);
            break;
        }

        FD_SET(serialFd, &currFdSet);
        nfds = max(serialFd, (connectedFd >= 0) ? connectedFd : listenFd) + 1;

        /* 
         * This is the select loop which waits for characters to be received on 
         * the serial/pty descriptor and on either the listen socket (meaning 
         * an incoming connection is queued) or on a connected socket 
         * descriptor.
         */
        while (1) {
            /* wait indefinitely for someone to blink */
            fd_set readFdSet = currFdSet;
            const int sel = select(nfds, &readFdSet, 0, 0, 0);

            if (sel == -1) {
                if (errno == EINTR) {
                    break;  /* drop out of inner while */
                } else {
                    fprintf(stderr, "select() returned -1, errno = %d\n", errno);
                    exit(1);
                }
            } else if (sel <= 0) {
                continue;
            }

            /* check for a new connection to accept */
            if (FD_ISSET(listenFd, &readFdSet)) {
                /* new connection is here, accept it */
                connectedFd = sioAcceptConnection(listenFd, addressFamily);
                FD_CLR(listenFd, &currFdSet);
                FD_SET(connectedFd, &currFdSet);
                nfds = max(serialFd, connectedFd) + 1;
            }

            /* check for packet received on the client socket */
            if ((connectedFd >= 0) && FD_ISSET(connectedFd, &readFdSet)) {
                /* connected tio_agent has something to relay to serial port */
                char msgBuff[128];
                const int readCount = sioSocketRead(connectedFd, msgBuff,
                    sizeof(msgBuff));
                if (readCount < 0) {
                    FD_CLR(connectedFd, &currFdSet);
                    FD_SET(listenFd, &currFdSet);
                    nfds = max(serialFd, listenFd) + 1;
                    connectedFd = -1;
                } else if (readCount > 0) {
                    sioTTYWrite(serialFd, msgBuff, readCount);
                }
            }

            /* check for a character on the serial port */
            if (FD_ISSET(serialFd, &readFdSet)) {
                /* 
                 * serial port has something to send to the tio_agent,
                 * if connected 
                 */
                char msgBuff[200];
                int serialRet = sioTTYRead(serialFd, msgBuff, sizeof(msgBuff),
                    &serialPos);
                if (serialRet < 0) {
                    /* fall out of this loop to reopen serial port or pts */
                    break;
                } else if ((serialRet > 0) && (connectedFd >= 0)) {
                    sioSocketWrite(connectedFd, msgBuff);
                }
            }
        }

        close(serialFd);
        FD_CLR(serialFd, &currFdSet);
    }

    if (verboseFlag) {
        printf("cleaning up\n");
    }

    if (connectedFd >= 0) {
        close(connectedFd);
    }
    if (listenFd >= 0) {
        close(listenFd);
    }

    if (tcpPort == 0) {
        /* try to remove the socket file, best effort */
        unlink(SIO_AGENT_UNIX_SOCKET);
    }
}
